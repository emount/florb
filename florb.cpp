#include <GL/glew.h>
#include <GL/glx.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "florb.h"
#include "florbUtils.h"
#include "nlohmann/json.hpp"

// TEMPORARY DEBUG
#include "stb_image.h"

namespace fs = std::filesystem;

using std::cerr;
using std::cos;
using std::cout;
using std::endl;
using std::exception;
using std::ifstream;
using std::lock_guard;
using std::mutex;
using std::pair;
using std::sin;
using std::string;
using std::vector;

using json = nlohmann::json_abi_v3_12_0::json;

const string Florb::k_DefaultImageDir("images");

const float Florb::k_SphereRadius(1.0f);

// Sphere rendering parameters for high-detail, production quality rendering
const int Florb::k_SectorCount(144);
const int Florb::k_StackCount(72);

// Florb class implementation
Florb::Florb() :
    fallbackTextureID(FlorbUtils::createDebugTexture()) {
    loadConfig();
    generateSphere(k_SphereRadius, k_SectorCount, k_StackCount);
    initShaders();
}

Florb::~Florb() {
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(shaderProgram);
}

void Florb::loadConfig() {
    ifstream file("florb.json");
    if (!file.is_open()) {
        cerr << "[WARN] Could not open florb.json for configuration" << endl;
        return;
    }

    json config;
    try {
        file >> config;

        if (config.contains("center") && config["center"].is_array() && config["center"].size() == 2) {
            setCenter(config["center"][0], config["center"][1]);
        }

        if (config.contains("zoom") && config["zoom"].is_number()) {
            setZoom(config["zoom"]);
        }

        if (config.contains("image_path") && config["image_path"].is_string()) {
            imagePath = config["image_path"];
        }

    } catch (const exception& exc) {
        cerr << "[ERROR] Failed to parse \"florb.json\" : " << exc.what() << endl;
    }
}

void Florb::loadFlowers(const string& directory) {
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
    lock_guard<mutex> lock(stateMutex);
    zoom = z;
}

void Florb::setCenter(float x, float y) {
    lock_guard<mutex> lock(stateMutex);
    offsetX = x;
    offsetY = y;
}

float Florb::getZoom() {
    lock_guard<mutex> lock(stateMutex);
    return zoom;
}

pair<float, float> Florb::getCenter() {
    lock_guard<mutex> lock(stateMutex);
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

    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    FlorbUtils::glCheck("glDrawElements()");
    
    glBindVertexArray(0);
}

void Florb::generateSphere(float radius, int sectorCount, int stackCount) {
    vector<float> vertices;
    vector<unsigned int> indices;

    const float PI = 3.14159265359f;

    for (int i = 0; i <= stackCount; ++i) {
        float stackAngle = PI / 2 - i * (PI / stackCount);  // from pi/2 to -pi/2
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);

        for (int j = 0; j <= sectorCount; ++j) {
            float sectorAngle = j * 2 * PI / sectorCount;  // from 0 to 2pi

            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);
            float u = (float)j / sectorCount;
            float v = (float)i / stackCount;

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(u);
            vertices.push_back(v);
        }
    }

    for (int i = 0; i < stackCount; ++i) {
        for (int j = 0; j < sectorCount; ++j) {
            int first = i * (sectorCount + 1) + j;
            int second = first + sectorCount + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
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

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Florb::initShaders() {
    const char* vertexShaderSource = R"glsl(
        #version 330 core
        
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec2 aTexCoord;
        out vec3 fragPos;
        
        uniform mat4 projView;
        
        void main()
        {
            fragPos = aPos; // Pass world-space position to fragment shader
            gl_Position = projView * vec4(aPos, 1.0);
        }
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
