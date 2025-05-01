#include <math.h>
#include <filesystem>
#include <iostream>

#include "florb.h"

namespace fs = std::filesystem;

using std::cos;
using std::sin;

// Florb class implementation
Florb::Florb() {
    createSphereGeometry();
    initShader();
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
    
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glViewport(0, 0, screenWidth, screenHeight);
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

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        glUniform1f(glGetUniformLocation(shaderProgram, "zoom"), zoom);
        glUniform2f(glGetUniformLocation(shaderProgram, "centerOffset"), offsetX, offsetY);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, flowers[currentFlower].getTextureID());
    glUniform1i(glGetUniformLocation(shaderProgram, "currentTexture"), 0);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
}

void Florb::createSphereGeometry() {
    const int X_SEGMENTS = 64;
    const int Y_SEGMENTS = 64;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    for (int y = 0; y <= Y_SEGMENTS; ++y) {
        for (int x = 0; x <= X_SEGMENTS; ++x) {
            float xSegment = (float)x / (float)X_SEGMENTS;
            float ySegment = (float)y / (float)Y_SEGMENTS;
            float xPos = std::cos(xSegment * 2.0f * 3.14159265f) * std::sin(ySegment * 3.14159265f);
            float yPos = std::cos(ySegment * 3.14159265f);
            float zPos = std::sin(xSegment * 2.0f * 3.14159265f) * std::sin(ySegment * 3.14159265f);

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

void Florb::initShader() {
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
        uniform float zoom;
        uniform vec2 centerOffset;
        void main()
        {
            vec3 dir = normalize(fragPos);
            vec2 uv;
            uv.x = atan(dir.z, dir.x) / (2.0 * 3.14159265) + 0.5;
            uv.y = asin(dir.y) / 3.14159265 + 0.5;
            uv = (uv - 0.5) * zoom + 0.5 + centerOffset;
            uv = clamp(uv, 0.0, 1.0);
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
