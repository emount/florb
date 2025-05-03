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

const string Florb::k_DefaultImagePath("images");

const float Florb::k_SphereRadius(1.0f);

// Sphere rendering parameters for high-detail, production quality rendering
const int Florb::k_SectorCount(144);
const int Florb::k_StackCount(72);

// Florb class implementation
Florb::Florb() :
    fallbackTextureID(FlorbUtils::createDebugTexture()) {
    loadConfig();
    loadFlowers();
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

        if (config.contains("image_path") && config["image_path"].is_string()) {
            imagePath = config["image_path"];
        } else {
            imagePath = k_DefaultImagePath;
        }

        if (config.contains("vignette") && config["vignette"].is_object()) {
	    const auto &vignette(config["vignette"]);

	    if (vignette.contains("radius") and vignette["radius"].is_number()) {
		setVignetteRadius(vignette["radius"]);
	    }

	    if (vignette.contains("exponent") and vignette["exponent"].is_number()) {
		setVignetteExponent(vignette["exponent"]);
	    }
        }

        if (config.contains("center") && config["center"].is_array() && config["center"].size() == 2) {
            setCenter(config["center"][0], config["center"][1]);
        }

        if (config.contains("zoom") && config["zoom"].is_number()) {
            auto zoomFactor(1.0f / static_cast<float>(config["zoom"]));
            setZoom(zoomFactor);
        }

    } catch (const exception& exc) {
        cerr << "[ERROR] Failed to parse \"florb.json\" : " << exc.what() << endl;
    }
}

void Florb::loadFlowers() {
    fs::path filepath(imagePath);
    if(fs::is_directory(filepath)) {
        for (const auto& entry : fs::directory_iterator(imagePath)) {
            if (entry.is_regular_file()) {
                flowers.emplace_back(entry.path().string());
            flowers.back().loadImage();
            }
        }
    } else {
        cerr << "Image path \"" << imagePath << "\" does not exist" << endl;
    }
}

void Florb::nextFlower() {
    if(++currentFlower >= flowers.size()) currentFlower = 0;
}

float Florb::getVignetteRadius() {
    lock_guard<mutex> lock(stateMutex);
    return vignetteRadius;
}

void Florb::setVignetteRadius(float r) {
    lock_guard<mutex> lock(stateMutex);
    vignetteRadius = r;
}

float Florb::getVignetteExponent() {
    lock_guard<mutex> lock(stateMutex);
    return vignetteExponent;
}

void Florb::setVignetteExponent(float r) {
    lock_guard<mutex> lock(stateMutex);
    vignetteExponent = r;
}

pair<float, float> Florb::getCenter() {
    lock_guard<mutex> lock(stateMutex);
    return {offsetX, offsetY};
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

void Florb::setZoom(float z) {
    lock_guard<mutex> lock(stateMutex);
    zoom = z;
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

    // Set offset and zoom parameters for textures
    GLuint resolutionLoc = glGetUniformLocation(shaderProgram, "resolution");
    GLuint offsetLoc = glGetUniformLocation(shaderProgram, "offset");
    GLuint zoomLoc = glGetUniformLocation(shaderProgram, "zoom");
    int vignetteRadiusLoc = glGetUniformLocation(shaderProgram, "vignetteRadius");
    int vignetteExponentLoc = glGetUniformLocation(shaderProgram, "vignetteExponent");
    glUniform2f(resolutionLoc, screenWidth, screenHeight);   
    glUniform1f(zoomLoc, zoom);
    glUniform2f(offsetLoc, offsetX, offsetY);   
    glUniform1f(vignetteRadiusLoc, vignetteRadius);
    glUniform1f(vignetteExponentLoc, vignetteExponent);

    // Activate texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glBindVertexArray(vao);
    FlorbUtils::glCheck("glBindVertexArray(vao)");

    glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
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
        out vec4 FragColor;
        
        in vec3 fragPos;
        uniform sampler2D currentTexture;
        uniform float zoom;
        uniform vec2 offset;
        uniform float vignetteRadius;
        uniform float vignetteExponent;
        uniform vec2 resolution;
        
        void main() {
            vec3 dir = normalize(fragPos);
        
            vec2 uv;
            uv.x = atan(dir.z, dir.x) / (2.0 * 3.14159265) + 0.5;
            uv.y = asin(dir.y) / 3.14159265 + 0.5;
        
            uv = (uv - 0.5) * zoom + 0.5 + offset;
            uv.y = 1.0 - uv.y;
        
            if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) discard;
            vec4 color = texture(currentTexture, uv);
        
            // Aspect-corrected center-relative coords
            vec2 screenUV = gl_FragCoord.xy / resolution;
            vec2 centered = screenUV - vec2(0.5);
            centered.x *= resolution.x / resolution.y;  // aspect correction
        
            float radius = length(centered);
            float fadeStart = 1.0 - vignetteRadius;
            float fadeEnd = 1.0;
        
            if (radius > fadeStart && radius <= fadeEnd) {
                // Zero to one in band
                float t = (radius - fadeStart) / (fadeEnd - fadeStart);
                float bands = floor(t * 10.0);
                if (mod(bands, 2.0) < 1.0) {
                    FragColor = vec4(t * color.r,
                                     t * color.g,
                                     t * color.b,
                                     1.0);
                } else {
                    // TEMPORARY
                    // Background color, pass this in as a variable later on
                    FragColor = vec4(0.1, 0.1, 0.1, 1.0);
                }
            } else {
                // Center region gets unmodified image data
                FragColor = color;
            }
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
