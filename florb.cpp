#include <GL/glew.h>
#include <GL/glx.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <math.h>
#include <filesystem>
#include <iostream>

#include "florb.h"
#include "florbUtils.h"

// TEMPORARY DEBUG
#include "stb_image.h"

namespace fs = std::filesystem;

using std::cerr;
using std::cout;
using std::endl;
using std::cos;
using std::sin;
using std::string;

const string Florb::k_DefaultImageDir("images");

// Florb class implementation
Florb::Florb() :
    fallbackTextureID(FlorbUtils::createDebugTexture()) {
    generateSphere();
    initShaders();
}

Florb::~Florb() {
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(shaderProgram);
}

void Florb::loadFlowers(const std::string& directory) {
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            flowers.emplace_back(entry.path().string());
        }
    }
}

void Florb::nextFlower() {
    if(++currentFlower >= flowers.size()) currentFlower = 0;
}

void Florb::setZoom(float z) {
    std::lock_guard<std::mutex> lock(stateMutex);
    zoom = z;
}

void Florb::setCenter(float x, float y) {
    std::lock_guard<std::mutex> lock(stateMutex);
    offsetX = x;
    offsetY = y;
}

float Florb::getZoom() {
    std::lock_guard<std::mutex> lock(stateMutex);
    return zoom;
}

std::pair<float, float> Florb::getCenter() {
    std::lock_guard<std::mutex> lock(stateMutex);
    return {offsetX, offsetY};
}

void Florb::renderFrame() {
    extern int screenWidth;
    extern int screenHeight;
    
    // FlorbUtils::glCheck("renderFrame()");
  
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glViewport(0, 0, screenWidth, screenHeight);
    // FlorbUtils::glCheck("glViewport()");
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shaderProgram);
    
    float aspect = (float)screenWidth / (float)screenHeight;    
    float projection[16] = {
        1.0f / aspect, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f,    0.0f, 0.0f,
        0.0f, 0.0f,   -1.0f, 0.0f,
        0.0f, 0.0f,    0.0f, 1.0f
    };
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, projection);
    // FlorbUtils::glCheck("glUniformMatrix4fv()");

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        glUniform1f(glGetUniformLocation(shaderProgram, "zoom"), zoom);
	// FlorbUtils::glCheck("glUniform1f(glGetUniformLocation(zoom)");
        glUniform2f(glGetUniformLocation(shaderProgram, "centerOffset"), offsetX, offsetY);
	// FlorbUtils::glCheck("glUniform2f(glGetUniformLocation(centerOffset)");
    }

    glActiveTexture(GL_TEXTURE0);
    FlorbUtils::glCheck("glActiveTexture()");

    cout << "Florb::renderFrame() - Rendering flower ["
 	 << flowers[currentFlower].getFilename()
 	 << "]"
 	 << endl;
    
    GLuint tex = flowers.empty() ? fallbackTextureID :
                                   flowers[currentFlower].getTextureID();

    /// TEST IMAGE LOAD TO TEXTURE
    stbi_set_flip_vertically_on_load(false);
    GLint width, height, channels;
    GLuint testTex = 0;
    const char* filename = "test/AvatarPic.png";
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 0);
    
    if (!data) {
        throw std::runtime_error("Failed to load image: " + string(filename));
    }

    GLenum format;
    if (channels == 1)
        format = GL_RED;
    else if (channels == 3)
        format = GL_RGB;
    else if (channels == 4)
        format = GL_RGBA;
    else
        throw std::runtime_error("Unsupported channel count");
    
    static_cast<void>(format);

    glGenTextures(1, &testTex);

    std::cerr << "[DEBUG] Loaded image : "
	      << filename
	      << " ("
	      << width
	      << "x"
	      << height
	      << "), "
	      << channels
	      << " channels, assigned texture ID ("
	      << testTex
	      << ")"
	      << endl;

    glBindTexture(GL_TEXTURE_2D, testTex);
    FlorbUtils::glCheck("glBindTexture()");
		    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    FlorbUtils::glCheck("glTexImage2D()");

    GLint texWidth = 0, texHeight = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texHeight);
    std::cerr << "Bound texture ID "
	      << testTex
	      << " has dimensions ["
	      << texWidth
	      << "x"
	      << texHeight
	      << "]"
	      << endl;

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[ERROR] OpenGL texture upload failed: " << gluErrorString(err) << "\n";
    }
    
    glGenerateMipmap(GL_TEXTURE_2D);

    // glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
    /// END TEST TEXTURE LOAD
    
    cerr << "[OVERRIDE] Overriding flower texture with test texture" << endl;

    tex = testTex;
    
    if (!glIsTexture(tex))
        cerr << "[WARN] Texture ID ("
	     << tex
	     << ") is not valid"
	     << endl;

    if (!glXGetCurrentContext()) {
        cerr << "[FATAL] OpenGL context is not current" << endl;
    }

    glBindTexture(GL_TEXTURE_2D, tex);
    FlorbUtils::glCheck("glBindTexture()");

    GLint bound;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    std::cerr << "[DEBUG] Current flower index: "
	      << currentFlower
	      << ", texture ID: "
	      << bound
	      << endl;

    GLint texLoc = glGetUniformLocation(shaderProgram, "currentTexture");
    if (texLoc >= 0) {
        glUniform1i(texLoc, 0);
    } else {
        cerr << "[ERROR] Uniform 'currentTexture' not found\n";
    }
    
    glUniform1i(texLoc, 0);
    // TEMPORARY REINSTATE SOON FlorbUtils::glCheck("glGetUniformLocation()()");

    GLint activeTex = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTex);
    // std::cerr << "[DEBUG] Active texture unit: " << (activeTex - GL_TEXTURE0) << "\n";

    glBindVertexArray(vao);
    // FlorbUtils::glCheck("glBindVertexArray()");

    GLint debugTex;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &debugTex);
    std::cerr << "[DEBUG] GL_TEXTURE_BINDING_2D: " << debugTex << "\n";

    glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
    // FlorbUtils::glCheck("glDrawElements()");
}

void Florb::generateSphere() {
    const int X_SEGMENTS = 64;
    const int Y_SEGMENTS = 64;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    int radius = 1.0f;

    for (int y = 0; y <= Y_SEGMENTS; ++y) {
        for (int x = 0; x <= X_SEGMENTS; ++x) {
            float xSegment = (float)x / (float)X_SEGMENTS;
            float ySegment = (float)y / (float)Y_SEGMENTS;
            float xPos = radius * std::cos(xSegment * 2.0f * 3.14159265f) * std::sin(ySegment * 3.14159265f);
            float yPos = radius * std::cos(ySegment * 3.14159265f);
            float zPos = radius * std::sin(xSegment * 2.0f * 3.14159265f) * std::sin(ySegment * 3.14159265f);

            vertices.push_back(xPos);
            vertices.push_back(yPos);
            vertices.push_back(zPos);
        }
    }

    for (int y = 0; y < Y_SEGMENTS; ++y) {
        for (int x = 0; x <= X_SEGMENTS; ++x) {
            indices.push_back(y * (X_SEGMENTS + 1) + x);
            indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
        }
    }

    indexCount = indices.size();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Florb::initShaders() {
    const char* vertexShaderSource = R"glsl(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        out vec3 fragPos;
        uniform mat4 projection;
        void main()
        {
            fragPos = aPos;
            gl_Position = projection * vec4(aPos, 1.0);
        }
    )glsl";

    const char* fragmentShaderSource = R"glsl(
        // GREEN STATIC #version 330 core
        // GREEN STATIC out vec4 FragColor;
        // GREEN STATIC uniform sampler2D currentTexture;
        // GREEN STATIC in vec2 vTexCoord;
        // GREEN STATIC 
        // GREEN STATIC void main() {
        // GREEN STATIC     vec4 color = texture(currentTexture, vTexCoord);
        // GREEN STATIC     FragColor = color;
        // GREEN STATIC }
        #version 330 core
        out vec4 FragColor;
        in vec3 fragPos;
        uniform sampler2D currentTexture;
        
        void main() {
            vec3 dir = normalize(fragPos);
            vec2 uv;
            uv.x = atan(dir.z, dir.x) / (2.0 * 3.14159265) + 0.5;
            uv.y = asin(dir.y) / 3.14159265 + 0.5;
            uv = clamp(uv, 0.001, 0.999);
            uv.y = 1.0 - uv.y;
            FragColor = texture(currentTexture, uv);
        }
    )glsl";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}
