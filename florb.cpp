#include <GL/glew.h>
#include <GL/glx.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
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
	    flowers.back().loadImage();
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
    
    FlorbUtils::glCheck("renderFrame()");

    if (!glXGetCurrentContext()) {
        cerr << "[renderFrame()] OpenGL context is not current" << endl;
    }

    // Set a dark gray background color for the window
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    GLuint textureID;
    if(flowers.empty()) {
        textureID = fallbackTextureID;
    } else {
        auto &flower = flowers[currentFlower];
	flower.loadImage();
	textureID = flower.getTextureID();
    }
    
    if (!glIsTexture(textureID))
        cerr << "[WARN] Texture ID ("
	     << textureID
	     << ") is not valid"
	     << endl;

    glUseProgram(shaderProgram);
    
    // Projection and view matrices
    float aspect = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f),
                                 glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 projView = projection * view;

    // Set the uniform once per frame
    int projViewLoc = glGetUniformLocation(shaderProgram, "projView");
    glUniformMatrix4fv(projViewLoc, 1, GL_FALSE, glm::value_ptr(projView));
    FlorbUtils::glCheck("glUniformMatrix4fv()");

    // Use texture unit 0
    int texLoc = glGetUniformLocation(shaderProgram, "currentTexture");
    glUniform1i(texLoc, 0);

    // Activate texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glBindVertexArray(vao);
    FlorbUtils::glCheck("glBindVertexArray(vao)");

    glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
    FlorbUtils::glCheck("glDrawElements()");
    
    glBindVertexArray(0);
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);
}

void Florb::initShaders() {
    const char* vertexShaderSource = R"glsl(
        #version 330 core
        
        layout(location = 0) in vec3 aPos;
        out vec3 fragPos;
        
        uniform mat4 projView;
        
        void main()
        {
            fragPos = aPos; // Pass world-space position to fragment shader
            gl_Position = projView * vec4(aPos, 1.0);
        }
//        #version 330 core
//        layout(location = 0) in vec3 aPos;
//        out vec3 fragPos;
//        uniform mat4 projView;
//        
//        void main()
//        {
//            fragPos = aPos;
//            gl_Position = projView * vec4(aPos, 1.0);
//        }
    )glsl";

    const char* fragmentShaderSource = R"glsl(
        #version 330 core
        
        in vec3 fragPos;
        out vec4 FragColor;
        
        uniform sampler2D currentTexture;
        
        void main()
        {
            vec3 dir = normalize(fragPos);
        
            // Convert 3D direction to spherical coordinates
            float u = atan(dir.z, dir.x) / (2.0 * 3.14159265) + 0.5;
            float v = asin(dir.y) / 3.14159265 + 0.5;
        
            vec2 uv = vec2(u, 1.0 - v); // Flip V
        
            // Uncomment to visualize UVs directly
            // FragColor = vec4(uv, 0.0, 1.0);
        
            FragColor = texture(currentTexture, uv);
        }
//        #version 330 core
//        out vec4 FragColor;
//        in vec3 fragPos;
//        
//        uniform sampler2D currentTexture;
//        uniform float zoom;
//        uniform vec2 centerOffset;
//        
//        void main()
//        {
//            vec3 dir = normalize(fragPos);
//            vec2 uv;
//            uv.x = atan(dir.z, dir.x) / (2.0 * 3.14159265) + 0.5;
//            uv.y = asin(dir.y) / 3.14159265 + 0.5;
//            uv = (uv - 0.5) * zoom + 0.5 + centerOffset;
//            uv = clamp(uv, 0.0, 1.0);
//            uv.y = 1.0 - uv.y;
//            FragColor = vec4(uv, 0.0, 1.0);
//            // FragColor = texture(currentTexture, uv);
//        }
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
