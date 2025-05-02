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

// Uncomment to enable class-level debug
// #define DEBUG_MESSAGES

namespace fs = std::filesystem;

using std::cerr;
using std::endl;
using std::cos;
using std::sin;
using std::string;

const string Florb::k_ImageDir("images");

// Florb class implementation
Florb::Florb() :
    fallbackTextureID(FlorbUtils::createDebugTexture()) {
    loadFlowers(k_ImageDir);
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
    
    FlorbUtils::glCheck("renderFrame()");
  
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glViewport(0, 0, screenWidth, screenHeight);
    FlorbUtils::glCheck("glViewport()");
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
    FlorbUtils::glCheck("glUniformMatrix4fv()");

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        glUniform1f(glGetUniformLocation(shaderProgram, "zoom"), zoom);
	FlorbUtils::glCheck("glUniform1f(glGetUniformLocation(zoom)");
        glUniform2f(glGetUniformLocation(shaderProgram, "centerOffset"), offsetX, offsetY);
	FlorbUtils::glCheck("glUniform2f(glGetUniformLocation(centerOffset)");
    }

    glActiveTexture(GL_TEXTURE0);
    FlorbUtils::glCheck("glActiveTexture()");
    
    GLuint tex = flowers.empty() ? fallbackTextureID :
                                   flowers[currentFlower].getTextureID();
#ifdef DEBUG_MESSAGES
    std::cerr << "[DEBUG] Texture ID: " << tex << "\n";
#endif

// TEMPORARY DEBUG    if (!glIsTexture(tex))
// TEMPORARY DEBUG        std::cerr << "[WARN] Not a valid texture ID!\n";
    glBindTexture(GL_TEXTURE_2D, tex);
// TEMPORARY DEBUG    FlorbUtils::glCheck("glBindTexture()");
    
    glUniform1i(glGetUniformLocation(shaderProgram, "currentTexture"), 0);
    FlorbUtils::glCheck("glGetUniformLocation()()");

    glBindVertexArray(vao);
    FlorbUtils::glCheck("glBindVertexArray()");

    glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
    FlorbUtils::glCheck("glDrawElements()");
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
