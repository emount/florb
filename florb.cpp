#include <GL/glew.h>
#include <GL/glx.h>
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

extern Display *display;
extern Window window;

using json = nlohmann::json_abi_v3_12_0::json;

const string Florb::k_DefaultTitle("Florb v0.3");

const string Florb::k_DefaultImagePath("images");

const float Florb::k_SphereRadius(0.8f);

// Sphere rendering parameters for high-detail, production quality rendering
const int Florb::k_SectorCount(144);
const int Florb::k_StackCount(72);
const int Florb::k_XSegments(64); // Smoother sphere - 128
const int Florb::k_YSegments(64); // Smoother sphere - 128


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

        if (config.contains("title") && config["title"].is_string()) {
            setTitle(config["title"]);
        } else {
            setTitle(k_DefaultTitle);
        }
    
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

void Florb::setTitle(const string &title) {
    lock_guard<mutex> lock(stateMutex);
    FlorbUtils::setWindowTitle(display, window, title);
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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    glEnable(GL_DEPTH_TEST);
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
    FlorbUtils::glCheck("glUseProgram");
    
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
    GLuint vignetteRadiusLoc = glGetUniformLocation(shaderProgram, "vignetteRadius");
    GLuint vignetteExponentLoc = glGetUniformLocation(shaderProgram, "vignetteExponent");
    GLuint lightDirLoc = glGetUniformLocation(shaderProgram, "lightDir");
    
    glUniform2f(resolutionLoc, screenWidth, screenHeight);   
    glUniform1f(zoomLoc, zoom);
    glUniform2f(offsetLoc, offsetX, offsetY);   
    glUniform1f(vignetteRadiusLoc, vignetteRadius);
    glUniform1f(vignetteExponentLoc, vignetteExponent);

    // TEMPORARY DEBUG
    glUniform3f(glGetUniformLocation(shaderProgram, "lightDir"), 0.0f, -1.0f, -1.0f);
    glUniform3f(glGetUniformLocation(shaderProgram, "lightColor"), 1.0f, 1.0f, 1.0f);
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 1.0f, 0.6f, 0.2f); // orange


    // TEMPORARY - Turn this into a config
    glUniform3f(lightDirLoc, 0.0, 0.5, 1.0); // 30 degrees above viewer

    // Activate texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glBindVertexArray(vao);
    FlorbUtils::glCheck("glBindVertexArray(vao)");

    // glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
    glDrawArrays(GL_POINTS, 0, indexCount);
    // glDrawElements(GL_POINTS, indexCount, GL_UNSIGNED_INT, 0);

    FlorbUtils::glCheck("glDrawElements()");
    
    glBindVertexArray(0);
    FlorbUtils::glCheck("glBindVertexArray(0) - A");
}

void Florb::generateSphere(float radius, int X_SEGMENTS, int Y_SEGMENTS) {
    std::vector<Vertex> vertices;

    for (int y = 0; y <= Y_SEGMENTS; ++y) {
	    // For lighting

        for (int x = 0; x <= X_SEGMENTS; ++x) {
            float xSegment = (float)x / X_SEGMENTS;
            float ySegment = (float)y / Y_SEGMENTS;
            float xPos = radius * std::cos(xSegment * 2.0f * M_PI) * std::sin(ySegment * M_PI);
            float yPos = radius * std::cos(ySegment * M_PI);
            float zPos = radius * std::sin(xSegment * 2.0f * M_PI) * std::sin(ySegment * M_PI);

            Vertex v;
            v.position = glm::vec3(xPos, yPos, zPos); // Position
            v.normal = glm::normalize(v.position); // For lighting
            vertices.push_back(v);
        }
    }

    indexCount = static_cast<int>(vertices.size());

    // for (int i = 0; i < 5; ++i)
    //     std::cout << "Normal " << i << ": " << glm::to_string(vertices[i].normal) << std::endl;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    // Vertex position attributes
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    // Vertex normal attributes
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Florb::initShaders() {
    const char* vertexShaderSource = R"glsl(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        
        uniform vec2 resolution;
        
        out vec3 fragNormal;
        
        void main()
        {
            vec3 pos = aPos;
            pos.x *= resolution.y / resolution.x;  // aspect correction
        
            fragNormal = normalize(pos);           // store aspect-corrected normal
        
            gl_Position = vec4(pos, 1.0);          // project directly to screen for debug
        }
//        #version 330 core
//        
//        layout(location = 0) in vec3 aPos;
//        layout(location = 1) in vec3 aNormal;
//        
//        out vec3 fragPos;
//        out vec3 fragNormal;
//        
//        uniform mat4 model;
//        uniform mat4 view;
//        uniform mat4 projection;
//        
//        void main() {
//            vec4 worldPos = model * vec4(aPos, 1.0);
//            fragPos = worldPos.xyz;
//        
//            mat3 normalMatrix = transpose(inverse(mat3(model)));
//            fragNormal = normalMatrix * aNormal;
//        
//            gl_Position = projection * view * worldPos;
//        }
    )glsl";

    const char* fragmentShaderSource = R"glsl(
        #version 330 core
        in vec3 fragNormal;
        out vec4 FragColor;
        
        uniform vec3 lightDir;
        
        void main() {
            float intensity = max(dot(normalize(fragNormal), normalize(lightDir)), 0.0);
            // FragColor = vec4(vec3(intensity), 1.0);
            FragColor = vec4(fragNormal * 0.5 + 0.5, 1.0);  // visualize normals
        }

//        #version 330 core
//        in vec3 fragPos;
//        in vec3 fragNormal;
//
//        out vec4 FragColor;
//        
//        uniform vec3 lightDir;
//        uniform vec3 lightColor;
//        uniform vec3 objectColor;
//
//        uniform sampler2D currentTexture;
//        uniform float zoom;
//        uniform vec2 offset;
//        uniform float vignetteRadius;
//        uniform float vignetteExponent;
//        uniform vec2 resolution;
//        
//        void main() {
//            vec3 dir = normalize(fragPos);
//        
//            vec2 uv;
//            uv.x = atan(dir.z, dir.x) / (2.0 * 3.14159265) + 0.5;
//            uv.y = asin(dir.y) / 3.14159265 + 0.5;
//        
//            uv = (uv - 0.5) * zoom + 0.5 + offset;
//            uv.y = 1.0 - uv.y;
//        
//            if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
//                discard;
//
//            vec4 color = texture(currentTexture, uv);
//        
//            // Aspect-corrected center-relative coords
//            vec2 screenUV = gl_FragCoord.xy / resolution;
//            vec2 centered = screenUV - vec2(0.5);
//            centered.x *= resolution.x / resolution.y;
//
//            // Diffuse lighting
//            vec3 norm = normalize(fragNormal);
//            vec3 light = normalize(-lightDir);
//            
//            float intensity = max(dot(norm, light), 0.0);
//        
//            // Vignette effect
//            float radius = length(centered);
//            float fadeStart = 1.0 - vignetteRadius;
//            float fadeEnd = 1.0;
//
//            float vignette;
//            if (radius > fadeStart && radius <= fadeEnd) {
//                // Zero to one in band
//                float t = (radius - fadeStart) / (fadeEnd - fadeStart);
//                float bands = floor(t * 10.0);
//
//                if (mod(bands, 2.0) < 1.0) {
//                    vignette = (1.0 - clamp(pow(10 * t, 2.0), 0.0, 1.0));
//                } else {
//                    // TEMPORARY
//                    // Background color, pass this in as a variable later on
//                    FragColor = vec4(0.1, 0.1, 0.1, 1.0);
//                    return;
//                }
//            } else {
//                // Center region gets unmodified image data
//                vignette = 1.0;
//            }
//
//            // vec3 diffuse = diff * lightColor;
//            // vec3 result = objectColor * diffuse;
//            // FragColor = vec4(result, 1.0);
//
//            // Factor in all weights
//            // FragColor = vec4(vignette * intensity * color.r,
//            //                  vignette * intensity * color.g,
//            //                  vignette * intensity * color.b,
//            //                  1.0);
//
//            FragColor = vec4(fragNormal * 0.5 + 0.5, 1.0);  // visualize normals as colors
//        }
    )glsl";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    FlorbUtils::glCheck("glCompileShader(vertexShader)");

    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
      cerr << "Vertex shader compilation error" << endl;    

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    FlorbUtils::glCheck("glCompileShader(vertexShader)");
    
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
      cerr << "Fragment shader compilation error" << endl;

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    FlorbUtils::glCheck("glLinkProgram()");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}
