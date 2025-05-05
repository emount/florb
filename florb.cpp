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
    lightDirection(3, 0.0f),
    lightColor(3, 0.0f),
    fallbackTextureID(FlorbUtils::createDebugTexture()) {
    loadConfigs();
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

void Florb::loadConfigs() {
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

        // Light configs
        if (config.contains("light") && config["light"].is_object()) {
            const auto &light(config["light"]);
            
            if (light.contains("direction") and light["direction"].is_array()) {
                const auto &direction(light["direction"]);
                setLightDirection(direction[0], direction[1], direction[2]);
            }
            
            if (light.contains("intensity") and light["intensity"].is_number()) {
                setLightIntensity(light["intensity"]);
            }
            
            if (light.contains("color") and light["color"].is_array()) {
                const auto &color(light["color"]);
                setLightColor(color[0], color[1], color[2]);
            }
            
        }

        // Vignette configs
        if (config.contains("vignette") && config["vignette"].is_object()) {
            const auto &vignette(config["vignette"]);
            
            if (vignette.contains("radius") and vignette["radius"].is_number()) {
                setVignetteRadius(vignette["radius"]);
            }
            
            if (vignette.contains("exponent") and vignette["exponent"].is_number()) {
                setVignetteExponent(vignette["exponent"]);
            }
        }

        // Pan / tilt / zoom configs
        if (config.contains("center") && config["center"].is_array() && config["center"].size() == 2) {
            setCenter(config["center"][0], config["center"][1]);
        }

        // TODO - Add tilt (center-axis rotation) config

        if (config.contains("zoom") && config["zoom"].is_number()) {
            auto zoomFactor(1.0f / static_cast<float>(config["zoom"]));
            setZoom(zoomFactor);
        }

        // Debug configs
        if (config.contains("debug") && config["debug"].is_object()) {
            const auto &debug(config["debug"]);

            if (debug.contains("render_mode") && debug["render_mode"].is_string()) {
                if(debug["render_mode"] == "fill") {
                    setRenderMode(RenderMode::FILL);
                } else if(debug["render_mode"] == "line") {
                    setRenderMode(RenderMode::LINE);
                } else {
		  cerr << "Invalid render_mode config value \""
		       << debug["render_mode"]
		       << "\""
		       << endl;
                }
            }
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

void Florb::setTitle(const string &title) {
    lock_guard<mutex> lock(stateMutex);
    FlorbUtils::setWindowTitle(display, window, title);
}

const vector<float>& Florb::getLightDirection() const {
    lock_guard<mutex> lock(stateMutex);
    return lightDirection;
}

void Florb::setLightDirection(float alpha, float beta, float phi) {
    lock_guard<mutex> lock(stateMutex);
    lightDirection[0] = alpha;
    lightDirection[1] = beta;
    lightDirection[2] = phi;
}

float Florb::getLightIntensity() const {
    lock_guard<mutex> lock(stateMutex);
    return lightIntensity;
}

void Florb::setLightIntensity(float i) {
    lock_guard<mutex> lock(stateMutex);
    lightIntensity = i;
}

const vector<float>& Florb::getLightColor() const {
    lock_guard<mutex> lock(stateMutex);
    return lightColor;
}

void Florb::setLightColor(float r, float g, float b) {
    lock_guard<mutex> lock(stateMutex);
    lightColor[0] = r;
    lightColor[1] = g;
    lightColor[2] = b;
}

float Florb::getVignetteRadius() const {
    lock_guard<mutex> lock(stateMutex);
    return vignetteRadius;
}

void Florb::setVignetteRadius(float r) {
    lock_guard<mutex> lock(stateMutex);
    vignetteRadius = r;
}

float Florb::getVignetteExponent() const {
    lock_guard<mutex> lock(stateMutex);
    return vignetteExponent;
}

void Florb::setVignetteExponent(float r) {
    lock_guard<mutex> lock(stateMutex);
    vignetteExponent = r;
}

pair<float, float> Florb::getCenter() const {
    lock_guard<mutex> lock(stateMutex);
    return {offsetX, offsetY};
}

void Florb::setCenter(float x, float y) {
    lock_guard<mutex> lock(stateMutex);
    offsetX = x;
    offsetY = y;
}

float Florb::getZoom() const {
    lock_guard<mutex> lock(stateMutex);
    return zoom;
}

void Florb::setZoom(float z) {
    lock_guard<mutex> lock(stateMutex);
    zoom = z;
}
  
const Florb::RenderMode& Florb::getRenderMode() const {
    lock_guard<mutex> lock(stateMutex);
    return renderMode;
}

void Florb::setRenderMode(const Florb::RenderMode &r) {
    lock_guard<mutex> lock(stateMutex);
    renderMode = r;
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

    // Set either line or fill rendering
    if (renderMode == RenderMode::LINE) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

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
    GLuint lightDirectionLoc = glGetUniformLocation(shaderProgram, "lightDir");
    GLuint lightColorLoc = glGetUniformLocation(shaderProgram, "lightColor");
    
    glUniform2f(resolutionLoc, screenWidth, screenHeight);   
    glUniform1f(zoomLoc, zoom);
    glUniform2f(offsetLoc, offsetX, offsetY);

    glUniform3f(lightDirectionLoc,
                lightDirection[0],
                lightDirection[1],
                lightDirection[2]);
    
    // Weight light color with intensity
    vector<float> actualColor = {
      (lightIntensity * lightColor[0]),
      (lightIntensity * lightColor[1]),
      (lightIntensity * lightColor[2])
    };
    glUniform3f(lightColorLoc, actualColor[0], actualColor[1], actualColor[2]);

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
    FlorbUtils::glCheck("glBindVertexArray(0) - A");
}

void Florb::generateSphere(float radius, int X_SEGMENTS, int Y_SEGMENTS) {
    std::vector<Vertex> vertices;
    vector<unsigned int> indices;

    for (int y = 0; y <= Y_SEGMENTS; ++y) {
        for (int x = 0; x <= X_SEGMENTS; ++x) {
            float xSegment = (float)x / X_SEGMENTS;
            float ySegment = (float)y / Y_SEGMENTS;
            float xPos = radius * std::cos(xSegment * 2.0f * M_PI) * std::sin(ySegment * M_PI);
            float yPos = radius * std::cos(ySegment * M_PI);
            float zPos = radius * std::sin(xSegment * 2.0f * M_PI) * std::sin(ySegment * M_PI);

            Vertex v;
            v.position = glm::vec3(xPos, yPos, zPos); // Position
            v.normal = glm::normalize(v.position); // For light
            vertices.push_back(v);
        }
    }

    // Generate indices
    for (int y = 0; y < Y_SEGMENTS; ++y) {
        for (int x = 0; x <= X_SEGMENTS; ++x) {
            indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
            indices.push_back(y * (X_SEGMENTS + 1) + x);
        }
    }

    indexCount = static_cast<int>(indices.size());

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Load vertices
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    FlorbUtils::glCheck("glBufferData(GL_ARRAY_BUFFER)");

    // Load indices
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    FlorbUtils::glCheck("glBufferData(GL_ELEMENT_ARRAY_BUFFER)");

    // Vertex position attributes
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    FlorbUtils::glCheck("glEnableVertexAttribArray(0)");

    // Vertex normal attributes
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    FlorbUtils::glCheck("glEnableVertexAttribArray(1)");

    glBindVertexArray(0);
    FlorbUtils::glCheck("glBindVertexArray(0)");
}

void Florb::initShaders() {
    const char* vertexShaderSource = R"glsl(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        
        uniform vec2 resolution;

        out vec2 fragUV;
        out vec3 fragPos;
        out vec3 fragNormal;
        
        void main()
        {
            // Aspect ratio correction
            vec3 pos = aPos;
            pos.x *= resolution.y / resolution.x;

            // Assign fragment position and normal
            fragPos = aPos;
            fragNormal = normalize(pos);

            // Generate spherical UV coordinates
            vec3 dir = normalize(aPos);
            fragUV.x = atan(dir.z, dir.x) / (2.0 * 3.14159265) + 0.5;
            fragUV.y = asin(dir.y) / 3.14159265 + 0.5;
        
            gl_Position = vec4(pos, 1.0);
        }
    )glsl";

    const char* fragmentShaderSource = R"glsl(
        #version 330 core
        in vec2 fragUV;
        in vec3 fragPos;
        in vec3 fragNormal;

        out vec4 FragColor;
        
        uniform vec3 lightDir;
        uniform vec3 lightColor;

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
        
            if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
                discard;

            vec4 color = texture(currentTexture, uv);
        
            // Aspect-corrected center-relative coords
            vec2 screenUV = gl_FragCoord.xy / resolution;
            vec2 centered = screenUV - vec2(0.5);
            centered.x *= resolution.x / resolution.y;

            // Diffuse lighting
            vec3 norm = normalize(fragNormal);
            vec3 light = normalize(-lightDir);
            
            // Vignette effect
            float radius = length(centered);
            float fadeStart = 1.0 - vignetteRadius;
            float fadeEnd = 1.0;

            float vignette;
            if (radius > fadeStart && radius <= fadeEnd) {
                // Zero to one in band
                float t = (radius - fadeStart) / (fadeEnd - fadeStart);
                float bands = floor(t * 10.0);

                if (mod(bands, 2.0) < 1.0) {
                    vignette = (1.0 - clamp(pow(10 * t, 2.0), 0.0, 1.0));
                } else {
                    // TEMPORARY
                    // Background color, pass this in as a variable later on
                    FragColor = vec4(0.1, 0.1, 0.1, 1.0);
                    return;
                }
            } else {
                // Center region gets unmodified image data
                vignette = 1.0;
            }

            // Exaggerated intensity
            float intensity = max(dot(normalize(fragNormal), normalize(lightDir)), 0.0) * 2.5;
            float gamma = 2.2;
            vec3 corrected = pow(vec3(intensity), vec3(1.0 / gamma));
            float diffuse = max(dot(normalize(fragNormal), normalize(lightDir)), 0.0);
            vec4 texColor = texture(currentTexture, fragUV);

            // Factor in all weights
            FragColor = vec4(vignette * intensity * lightColor.r * color.r,
                             vignette * intensity * lightColor.g * color.g,
                             vignette * intensity * lightColor.b * color.b,
                             1.0);
            // FragColor = vec4(vignette * intensity * color.r,
            //                  vignette * intensity * color.g,
            //                  vignette * intensity * color.b,
            //                  1.0);
        }
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
