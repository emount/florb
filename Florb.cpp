#include <GL/glew.h>
#include <GL/glx.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "Florb.h"
#include "FlorbUtils.h"
#include "SinusoidalMotion.h"
#include "nlohmann/json.hpp"


namespace chrono = std::chrono;
namespace fs = std::filesystem;

using chrono::duration_cast;
using chrono::milliseconds;
using chrono::steady_clock;

using std::cerr;
using std::cos;
using std::cout;
using std::endl;
using std::exception;
using std::ifstream;
using std::lock_guard;
using std::make_shared;
using std::mt19937;
using std::mutex;
using std::pair;
using std::random_device;
using std::sin;
using std::string;
using std::uniform_real_distribution;
using std::vector;


extern Display *display;
extern Window window;

using json = nlohmann::json_abi_v3_12_0::json;

const string Florb::k_DefaultTitle("Florb v0.3");

const string Florb::k_DefaultImagePath("images");

const float Florb::k_MaxVideoFrameRate(120.0f);

const float Florb::k_DefaultVideoFrameRate(60.0f);

const float Florb::k_DefaultImageSwitch(5.0f);

const float Florb::k_DefaultRadius(0.8f);

const int Florb::k_MaxMotes(128);


// Florb class implementation
Florb::Florb() :
    imagePath(k_DefaultImagePath),

    videoFrameRate(k_DefaultVideoFrameRate),
    imageSwitch(k_DefaultImageSwitch),

    cameraView(3, 0.0f),
    zoom(1.0f),
    
    flowers(),
    flowerPaths(),
    currentFlower(0UL),

    offsetX(0.0f),
    offsetY(0.0f),
    baseRadius(k_DefaultRadius),
    radius(k_DefaultRadius),
    
    smoothness(7UL),

    bounceEnabled(false),
    bounceAmplitude(0.0f),
    bounceFrequency(0.0f),

    breatheEnabled(false),
    breatheAmplitude(2, k_DefaultRadius),
    breatheFrequency(0.0f),
    
    lightDirection(3, 0.0f),
    lightIntensity(1.0f),
    shininess(1.0f),
    lightColor(3, 0.0f),
    baseRimStrength(0.0f),
    rimStrength(0.0f),
    rimExponent(0.0f),
    rimColor(3, 0.0f),
    rimFrequency(0.0f),
    rimAnimateEnabled(false),
    animatedRimColor(0.0f, 0.0f, 0.0f),
    rimAnimateFrequency(0.0f),
    
    moteCount(0UL),
    moteRadii(),
    moteSpeeds(),
    moteMaxStep(),
    moteCenters(),
    moteDirections(),
    moteColor(3, 0.0f),


    bouncer(make_shared<SinusoidalMotion>()),
    bounceOffset(0.0f),
    breather(make_shared<SinusoidalMotion>()),
    rimPulser(make_shared<SinusoidalMotion>()),

    renderMode(RenderMode::FILL),
    specularMode(SpecularMode::NORMAL),
    
    fallbackTextureID(FlorbUtils::createDebugTexture()),
    
    dist(0.0f, 1.0f),

    stateMutex() {
  
    loadConfigs();
    loadFlowers();
    initShaders();

    // Seed the Mersenne Twister
    gen.seed(rd());
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

        // Title config
        if (config.contains("title") && config["title"].is_string()) {
            setTitle(config["title"]);
        } else {
            setTitle(k_DefaultTitle);
        }

        
        // Image path config
        if (config.contains("image_path") && config["image_path"].is_string()) {
            imagePath = config["image_path"];
        } else {
            imagePath = k_DefaultImagePath;
        }


        // Video configs
        if (config.contains("video") && config["video"].is_object()) {
            const auto &video(config["video"]);
            
            if (video.contains("frame_rate") and video["frame_rate"].is_number()) {
                setVideoFrameRate(video["frame_rate"]);
            }
            
            if (video.contains("image_switch") and video["image_switch"].is_number()) {
                setImageSwitch(video["image_switch"]);
            }
        }


        // Geometry configs
        if (config.contains("geometry") && config["geometry"].is_object()) {
            const auto &geometry(config["geometry"]);

            if (geometry.contains("center") && geometry["center"].is_array() && geometry["center"].size() == 2) {
                setCenter(geometry["center"][0], geometry["center"][1]);
            }

            if (geometry.contains("radius") and geometry["radius"].is_number()) {
                setRadius(geometry["radius"]);
            }
            
            if (geometry.contains("smoothness") and geometry["smoothness"].is_number()) {
                setSmoothness(geometry["smoothness"]);
            }
        }        


        // Effects configs
        if (config.contains("effects") && config["effects"].is_object()) {
            const auto &effects(config["effects"]);

            if (effects.contains("bounce") && effects["bounce"].is_object()) {
                const auto &bounce(effects["bounce"]);

                if (bounce.contains("enabled") and bounce["enabled"].is_boolean()) {
                    setBounceEnabled(bounce["enabled"]);
                }

                if (bounce.contains("amplitude") and bounce["amplitude"].is_number()) {
                    const auto &amplitude(bounce["amplitude"]);
                    setBounceAmplitude(amplitude);
                }
                
                if (bounce.contains("frequency") and bounce["frequency"].is_number()) {
                    setBounceFrequency(bounce["frequency"]);
                }
            }

            if (effects.contains("breathe") && effects["breathe"].is_object()) {
                const auto &breathe(effects["breathe"]);

                if (breathe.contains("enabled") and breathe["enabled"].is_boolean()) {
                    setBreatheEnabled(breathe["enabled"]);
                }

                if (breathe.contains("amplitude") and breathe["amplitude"].is_array()) {
                    const auto &amplitude(breathe["amplitude"]);
                    setBreatheAmplitude(amplitude[0], amplitude[1]);
                }
                
                if (breathe.contains("frequency") and breathe["frequency"].is_number()) {
                    setBreatheFrequency(breathe["frequency"]);
                }
            }
        }    
        
        // Camera configs
        if (config.contains("camera") && config["camera"].is_object()) {
            const auto &camera(config["camera"]);
            
            if (camera.contains("view") and camera["view"].is_array()) {
                const auto &view(camera["view"]);
                setCameraView(view[0], view[1], view[2]);
            }

            if (camera.contains("zoom") && camera["zoom"].is_number()) {
                auto zoomFactor(1.0f / static_cast<float>(camera["zoom"]));
                setZoom(zoomFactor);
            }
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
            
            if (light.contains("shininess") and light["shininess"].is_number()) {
                setShininess(light["shininess"]);
            }
            
            if (light.contains("color") and light["color"].is_array()) {
                const auto &color(light["color"]);
                setLightColor(color[0], color[1], color[2]);
            }

            // Rim lighting
            if (light.contains("rim") && light["rim"].is_object()) {
                const auto &rim(light["rim"]);

                if (rim.contains("strength") and rim["strength"].is_number()) {
                    setRimStrength(rim["strength"]);
                }
                
                if (rim.contains("exponent") and rim["exponent"].is_number()) {
                    setRimExponent(rim["exponent"]);
                }
            
		if (rim.contains("color") and rim["color"].is_array()) {
		    const auto &color(rim["color"]);
		    setRimColor(color[0], color[1], color[2]);
		}
                
                if (rim.contains("frequency") and rim["frequency"].is_number()) {
                    setRimFrequency(rim["frequency"]);
                }
                
                if (rim.contains("animate") and rim["animate"].is_object()) {
                    const auto &animate(rim["animate"]);

                    if (animate.contains("enabled") and animate["enabled"].is_boolean()) {
                        setRimAnimateEnabled(animate["enabled"]);
                    }
                    
                    if (animate.contains("frequency") and animate["frequency"].is_number()) {
                        setRimAnimateFrequency(animate["frequency"]);
                    }
                } // animate configs
            } // rim configs
        } // light configs

        
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


        // Mote configs
        if (config.contains("motes") && config["motes"].is_object()) {
            const auto &motes(config["motes"]);

            unsigned int count(0UL);
            if (motes.contains("count") and motes["count"].is_number_integer()) {
                count = motes["count"];
            }
            
            float radius(1.0f);
            if (motes.contains("radius") and motes["radius"].is_number()) {
                radius = motes["radius"];
            }

            float maxStep(1.0f);
            if (motes.contains("max_step") and motes["max_step"].is_number()) {
                maxStep = motes["max_step"];
            }

            vector<float> color(3, 0.0f);
            if (motes.contains("color") and motes["color"].is_array()) {
                color = vector<float>(motes["color"]);
            }
            
            initMotes(count, radius, maxStep, color);
        }


        // TODO - Add tilt (center-axis rotation) config

        
        // Debug configs
        if (config.contains("debug") && config["debug"].is_object()) {
            const auto &debug(config["debug"]);

            // Rendering mode - normal fill, or wiremesh lines
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

            // Specular mode - normal reflections, or debug spot
            if (debug.contains("specular_mode") && debug["specular_mode"].is_string()) {
                if(debug["specular_mode"] == "normal") {
                    setSpecularMode(SpecularMode::NORMAL);
                } else if(debug["specular_mode"] == "debug") {
                    setSpecularMode(SpecularMode::DEBUG);
                } else {
                    cerr << "Invalid specular_mode config value \""
                         << debug["specular_mode"]
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
            }
        }
    } else {
        cerr << "Image path \"" << imagePath << "\" does not exist" << endl;
    }
}

void Florb::nextFlower() {
    if(++currentFlower >= flowers.size()) currentFlower = 0;
}


// Title mutator

void Florb::setTitle(const string &title) {
    lock_guard<mutex> lock(stateMutex);
    FlorbUtils::setWindowTitle(display, window, title);
}


// Video accessors / mutators

float Florb::getVideoFrameRate() const {
    lock_guard<mutex> lock(stateMutex);
    return videoFrameRate;
}

void Florb::setVideoFrameRate(float r) {
    lock_guard<mutex> lock(stateMutex);

    if (videoFrameRate >= k_MaxVideoFrameRate) {
        videoFrameRate = k_MaxVideoFrameRate;
    } else {
        videoFrameRate = r;
    }
}

float Florb::getImageSwitch() const {
    lock_guard<mutex> lock(stateMutex);
    return imageSwitch;
}

void Florb::setImageSwitch(float s) {
    lock_guard<mutex> lock(stateMutex);

    imageSwitch = s;
}


// Camera accessors / mutators

const vector<float>& Florb::getCameraView() const {
    lock_guard<mutex> lock(stateMutex);
    return cameraView;
}

void Florb::setCameraView(float alpha, float beta, float phi) {
    lock_guard<mutex> lock(stateMutex);
    cameraView[0] = alpha;
    cameraView[1] = beta;
    cameraView[2] = phi;
}

float Florb::getZoom() const {
    lock_guard<mutex> lock(stateMutex);
    return zoom;
}

void Florb::setZoom(float z) {
    lock_guard<mutex> lock(stateMutex);
    zoom = z;
}


// Geometry accessors / mutators

pair<float, float> Florb::getCenter() const {
    lock_guard<mutex> lock(stateMutex);
    return {offsetX, offsetY};
}

void Florb::setCenter(float x, float y) {
    lock_guard<mutex> lock(stateMutex);
    offsetX = x;
    offsetY = y;
}

float Florb::getRadius() const {
    lock_guard<mutex> lock(stateMutex);
    return radius;
}

void Florb::setRadius(float r) {
    lock_guard<mutex> lock(stateMutex);
    baseRadius = radius = r;
    createBreather();
}

unsigned int Florb::getSmoothness() const {
    lock_guard<mutex> lock(stateMutex);
    return smoothness;
}

void Florb::setSmoothness(unsigned int s) {
    lock_guard<mutex> lock(stateMutex);
    smoothness = s;
}


// Bounce accessors / mutators

bool Florb::getBounceEnabled() const {
    lock_guard<mutex> lock(stateMutex);
    return bounceEnabled;
}

void Florb::setBounceEnabled(bool e) {
    lock_guard<mutex> lock(stateMutex);
    bounceEnabled = e;
    createBouncer();
}

float Florb::getBounceAmplitude() const {
    lock_guard<mutex> lock(stateMutex);
    return bounceAmplitude;
}

void Florb::setBounceAmplitude(float a) {
    lock_guard<mutex> lock(stateMutex);
    bounceAmplitude = a;
    createBouncer();
}

float Florb::getBounceFrequency() const {
    lock_guard<mutex> lock(stateMutex);
    return bounceFrequency;
}

void Florb::setBounceFrequency(float f) {
    lock_guard<mutex> lock(stateMutex);
    bounceFrequency = f;
    createBouncer();
}


// Breathe accessors / mutators

bool Florb::getBreatheEnabled() const {
    lock_guard<mutex> lock(stateMutex);
    return breatheEnabled;
}

void Florb::setBreatheEnabled(bool e) {
    lock_guard<mutex> lock(stateMutex);
    breatheEnabled = e;
    createBreather();
}

const vector<float>& Florb::getBreatheAmplitude() const {
    lock_guard<mutex> lock(stateMutex);
    return breatheAmplitude;
}

void Florb::setBreatheAmplitude(float min, float max) {
    lock_guard<mutex> lock(stateMutex);
    breatheAmplitude[0] = min;
    breatheAmplitude[1] = max;
    createBreather();
}

float Florb::getBreatheFrequency() const {
    lock_guard<mutex> lock(stateMutex);
    return breatheFrequency;
}

void Florb::setBreatheFrequency(float f) {
    lock_guard<mutex> lock(stateMutex);
    breatheFrequency = f;
    createBreather();
}


// Light accessors / mutators

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

float Florb::getShininess() const {
    lock_guard<mutex> lock(stateMutex);
    return shininess;
}

void Florb::setShininess(float s) {
    lock_guard<mutex> lock(stateMutex);
    shininess = s;
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


// Rim light accessors / mutators

float Florb::getRimStrength() const {
    lock_guard<mutex> lock(stateMutex);
    return baseRimStrength;
}

void Florb::setRimStrength(float s) {
    lock_guard<mutex> lock(stateMutex);
    baseRimStrength = s;
}

float Florb::getRimExponent() const {
    lock_guard<mutex> lock(stateMutex);
    return rimExponent;
}

void Florb::setRimExponent(float s) {
    lock_guard<mutex> lock(stateMutex);
    rimExponent = s;
}

const vector<float>& Florb::getRimColor() const {
    lock_guard<mutex> lock(stateMutex);
    return rimColor;
}

void Florb::setRimColor(float r, float g, float b) {
    lock_guard<mutex> lock(stateMutex);
    rimColor[0] = r;
    rimColor[1] = g;
    rimColor[2] = b;
}

float Florb::getRimFrequency() const {
    lock_guard<mutex> lock(stateMutex);
    return rimFrequency;
}

void Florb::setRimFrequency(float f) {
    lock_guard<mutex> lock(stateMutex);
    rimFrequency = f;
    createRimPulser();
}

bool Florb::getRimAnimateEnabled() const {
    lock_guard<mutex> lock(stateMutex);
    return rimAnimateEnabled;
}

void Florb::setRimAnimateEnabled(bool a) {
    lock_guard<mutex> lock(stateMutex);
    rimAnimateEnabled = a;
}

float Florb::getRimAnimateFrequency() const {
    lock_guard<mutex> lock(stateMutex);
    return rimAnimateFrequency;
}

void Florb::setRimAnimateFrequency(float f) {
    lock_guard<mutex> lock(stateMutex);
    rimAnimateFrequency = f;
}


// Vignette accessors / mutators

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


// Debug accessors / mutators

const Florb::RenderMode& Florb::getRenderMode() const {
    lock_guard<mutex> lock(stateMutex);
    return renderMode;
}

void Florb::setRenderMode(const Florb::RenderMode &r) {
    lock_guard<mutex> lock(stateMutex);
    renderMode = r;
}

const Florb::SpecularMode& Florb::getSpecularMode() const {
    lock_guard<mutex> lock(stateMutex);
    return specularMode;
}

void Florb::setSpecularMode(const Florb::SpecularMode &s) {
    lock_guard<mutex> lock(stateMutex);
    specularMode = s;
}


// Render frame method

void Florb::renderFrame() {
    extern int screenWidth;
    extern int screenHeight;
    static bool firstFrame(true);
    
    FlorbUtils::glCheck("renderFrame()");

    if (!glXGetCurrentContext()) {
        cerr << "[renderFrame()] OpenGL context is not current" << endl;
    }

    // Update physical effects
    updatePhysicalEffects();
    
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

    // Load all flower images upon the first frame
    if (firstFrame) {
        for (auto &flower : flowers) {
          flower.loadImage();
        }
    }
    
    GLuint textureID;
    if(flowers.empty()) {
        textureID = fallbackTextureID;
    } else {
        auto &flower = flowers[currentFlower];
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


    // Set screen resolution uniform
    GLuint resolutionLoc = glGetUniformLocation(shaderProgram, "resolution");
    glUniform2f(resolutionLoc, screenWidth, screenHeight);
    
    
    // Set offset and radius uniforms
    GLuint offsetLoc = glGetUniformLocation(shaderProgram, "offset");
    GLuint radiusLoc = glGetUniformLocation(shaderProgram, "radius");
    glUniform2f(offsetLoc, offsetX, offsetY);
    glUniform1f(radiusLoc, radius);
    

    // TODO - Move zoom uniform to camera uniforms
    GLuint zoomLoc = glGetUniformLocation(shaderProgram, "zoom");
    glUniform1f(zoomLoc, zoom);


    // Set light uniforms
    GLuint lightDirectionLoc = glGetUniformLocation(shaderProgram, "lightDir");
    GLuint lightColorLoc = glGetUniformLocation(shaderProgram, "lightColor");    
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


    // Set specular reflection uniforms
    GLuint viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
    GLuint shininessLoc = glGetUniformLocation(shaderProgram, "shininess");
    GLuint specularDebugLoc = glGetUniformLocation(shaderProgram, "specularDebug");

    glUniform3f(viewPosLoc,
                cameraView[0],
                cameraView[1],
                cameraView[2]);
                
    glUniform1f(shininessLoc, shininess);

    int specularDebug;
    if (specularMode == SpecularMode::NORMAL) {
        specularDebug = 0;
    } else {
        specularDebug = 1;
    }
    glUniform1i(specularDebugLoc, specularDebug);


    // Set rim lighting uniforms
    GLuint rimColorLoc = glGetUniformLocation(shaderProgram, "rimColor");
    GLuint rimExponentLoc = glGetUniformLocation(shaderProgram, "rimExponent");
    GLuint rimStrengthLoc = glGetUniformLocation(shaderProgram, "rimStrength");
    glUniform3f(rimColorLoc,
                animatedRimColor[0],
                animatedRimColor[1],
                animatedRimColor[2]);
    glUniform1f(rimExponentLoc, rimExponent);
    glUniform1f(rimStrengthLoc, rimStrength);


    // Set vignette effect uniforms
    GLuint vignetteRadiusLoc = glGetUniformLocation(shaderProgram, "vignetteRadius");
    GLuint vignetteExponentLoc = glGetUniformLocation(shaderProgram, "vignetteExponent");
    glUniform1f(vignetteRadiusLoc, vignetteRadius);
    glUniform1f(vignetteExponentLoc, vignetteExponent);

    
    // Dust mote uniforms
    GLuint moteCountLoc = glGetUniformLocation(shaderProgram, "moteCount");
    GLuint moteRadiiLoc = glGetUniformLocation(shaderProgram, "moteRadii");
    GLuint moteSpeedsLoc = glGetUniformLocation(shaderProgram, "moteSpeeds");
    GLuint moteCentersLoc = glGetUniformLocation(shaderProgram, "moteCenters");
    GLuint moteColorLoc = glGetUniformLocation(shaderProgram, "moteColor");
    glUniform1i(moteCountLoc, moteCount);
    glUniform1fv(moteRadiiLoc, moteRadii.size(), moteRadii.data());
    glUniform1fv(moteSpeedsLoc, moteSpeeds.size(), moteSpeeds.data());
    glUniform2fv(moteCentersLoc, moteCenters.size(), moteCenters.data());
    glUniform3f(moteColorLoc, moteColor[0], moteColor[1], moteColor[2]);


    // Bounce offset uniform
    GLuint bounceOffsetLoc = glGetUniformLocation(shaderProgram, "bounceOffset");
    glUniform1f(bounceOffsetLoc, bounceOffset);

    
    // Activate texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glBindVertexArray(vao);
    FlorbUtils::glCheck("glBindVertexArray(vao)");

    glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
    FlorbUtils::glCheck("glDrawElements()");
    
    glBindVertexArray(0);
    FlorbUtils::glCheck("glBindVertexArray(0) - A");

    firstFrame = false;
}


// Sphere generation method

void Florb::generateSphere(float radius, int sectorCount, int stackCount) {
    std::vector<Vertex> vertices;
    vector<unsigned int> indices;

    for (int y = 0; y <= stackCount; ++y) {
        for (int x = 0; x <= sectorCount; ++x) {
            float xSegment = static_cast<float>(x) / sectorCount;
            float ySegment = static_cast<float>(y) / stackCount;
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
    for (int y = 0; y < stackCount; ++y) {
        for (int x = 0; x <= sectorCount; ++x) {
            indices.push_back((y + 1) * (sectorCount + 1) + x);
            indices.push_back(y * (sectorCount + 1) + x);
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


// Shader program initialization

void Florb::initShaders() {
    const char* vertexShaderSource = R"glsl(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        
        out vec2 fragUV;
        out vec3 fragPos;
        out vec3 fragNormal;

        uniform float bounceOffset;

        uniform vec2 resolution;

        void main()
        {
            // Bounce offset addition
            vec3 pos = aPos + vec3(0.0, bounceOffset, 0.0);
        
            // Aspect ratio correction
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

        uniform float radius;

        uniform int moteCount;

        uniform float time;
        uniform float moteRadii[128];
        uniform float moteSpeeds[128];
        uniform vec2 moteCenters[128];
        uniform vec3 moteColor;

        uniform vec3 rimColor;
        uniform float rimExponent;
        uniform float rimStrength;
        
        uniform float vignetteRadius;
        uniform float vignetteExponent;
        uniform float zoom;
        
        uniform vec2 offset;
        uniform vec2 resolution;

        uniform vec3 lightDir;
        uniform vec3 lightColor;

        uniform vec3 viewPos;
        uniform float shininess;
        uniform int specularDebug;

        uniform sampler2D currentTexture;
        
        void main() {
            vec3 dir = normalize(fragPos);

            // Fragment location
            vec2 uv;
            uv.x = atan(dir.z, dir.x) / (2.0 * 3.14159265) + 0.5;
            uv.y = asin(dir.y) / 3.14159265 + 0.5;
        
            uv = (uv - 0.5) * zoom + 0.5 + offset;
            uv.y = 1.0 - uv.y;


            // Discard any pixel locations beyond the radius of the sphere
            if (length(fragPos) > radius)
                discard;


            // Dust mote contributions
            float dust = 0.0;
            for (int i = 0; i < moteCount; ++i) {
                float speed = moteSpeeds[i]; // pre-randomized per mote
                float radius = moteRadii[i] / resolution.y;
            
                // Wobbling orbit using sin/cos with time
                float wobbleX = sin(time * speed);
                float wobbleY = cos(time * speed * 0.5);
            
                vec2 orbitOffset = vec2(wobbleX, wobbleY) * 0.01;

                // Map [-1,1] to [0,1]
                vec2 motePos = (moteCenters[i] * 0.5 + 0.5);
                motePos += orbitOffset;
            
                float dist = distance(uv, motePos);
                float alpha = smoothstep(radius, 0.0, dist);
                dust += alpha;
            }

            // Aspect-corrected center-relative coords
            vec2 screenUV = gl_FragCoord.xy / resolution;
            vec2 centered = screenUV - vec2(0.5);
            centered.x *= resolution.x / resolution.y;


            // Diffuse lighting
            vec3 norm = normalize(fragNormal);
            vec3 light = normalize(-lightDir);


            // Normalize lighting components
            vec3 viewDir = normalize(viewPos - fragPos);
            vec3 reflectDir = reflect(-light, norm);


            // Calculate rim lighting
            float rim = pow(1.0 - max(dot(viewDir, normalize(fragNormal)), 0.0), rimExponent);
            vec3 rimLight = rimStrength * rim * rimColor;


            // Vignette effect
            float radial = length(centered);
            float fadeStart = 1.0 - vignetteRadius;
            float fadeEnd = 1.0;
            
            float vignette = 1.0;
            if (radial > fadeStart) {
                float t = (radial - fadeStart) / (fadeEnd - fadeStart);
                vignette = 1.0 - clamp(pow(t, vignetteExponent), 0.0, 1.0);
            }


            // Sample texture color
            vec4 texColor = texture(currentTexture, uv);


            // Diffuse lighting
            float diffuse = max(dot(fragNormal, lightDir), 0.0);


            // Amplified specular component
            float specular = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
            specular *= 4.0;


            // Final lighting contribution
            vec3 lighting = clamp((diffuse + specular) * lightColor, 0.0, 1.0);


            // Compute final color
            vec3 finalColor = vignette * lighting * texColor.rgb;
            finalColor += rimLight;

            // Clamp and overlay colored dust mote glow
            finalColor += clamp(dust, 0.0, 1.0) * moteColor;

            // Assign final color, taking debug modes into account
            if (specularDebug != 0) {
               FragColor = vec4(vec3(specular), 1.0);
            } else {
                FragColor = vec4(finalColor, 1.0);
            }
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
    FlorbUtils::glCheck("glCompileShader(fragmentShader)");
    
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


// Dust mote initialization method

void Florb::initMotes(unsigned int count,
                      float radius,
                      float maxStep,
                      const vector<float> &color) {
    // Randomize dust mote centers
    moteCount = count;
    moteCenters.resize(2 * moteCount);
    for (auto i = 0UL; i < (2 * moteCount); i++) {
        // Sample in [-1.0, 1.0] to cover full UV space after offset/zoom
        moteCenters[i] = ((2.0f * dist(gen)) - 1.0f);
        
        if ((i % 2) == 0) {
            float bias(dist(gen));
            if (bias > 0.5f) {
                moteDirections.push_back(1.0f);
            } else {
                moteDirections.push_back(-1.0f);
            }
        } else moteDirections.push_back(1.0f);
    }

    moteRadii = vector<float>(moteCount, radius);
    moteSpeeds = vector<float>(moteCount, maxStep);
    moteMaxStep = maxStep;
    moteColor = color;
}


// Physical effects methods

void Florb::createBouncer() {
    float phase(0.0f);

    float bias = bounceEnabled ? bounceAmplitude : 0.0f;
    float amplitude = bounceEnabled ? bounceAmplitude : 0.0f;
    bouncer = make_shared<SinusoidalMotion>(
        bounceEnabled,
        bias,
        amplitude,
        bounceFrequency,
        phase
    );
}

void Florb::createBreather() {
    float phase(M_PI / 2);
    float minR = breatheAmplitude[0];
    float maxR = breatheAmplitude[1];
    float bias = 0.5f * (minR + maxR);
    float amplitude = 0.5f * (maxR - minR);

    breather = make_shared<SinusoidalMotion>(
        breatheEnabled,
        bias,
        amplitude,
        breatheFrequency,
        phase
    );
}

void Florb::createRimPulser() {
    bool enabled(true);
    float phase(0.0f);

    rimPulser = make_shared<SinusoidalMotion>(
        enabled,
        baseRimStrength,
        baseRimStrength,
        rimFrequency,
        phase
    );
}


// Update physical effects method

void Florb::updatePhysicalEffects() {
    // Update the time uniform for physical effects
    static steady_clock::time_point startTime = steady_clock::now();
    float timeMsec = duration_cast<milliseconds>(steady_clock::now() - startTime).count();
    float timeSeconds = (timeMsec / 1000.0f);

    GLuint timeLoc = glGetUniformLocation(shaderProgram, "time");
    glUniform1f(timeLoc, timeSeconds);


    // Update the sphere
    bounceOffset = bouncer->evaluate(timeSeconds);
    auto breatheRadius(breather->evaluate(timeSeconds));
    setVignetteRadius(breatheRadius);

    GLuint vignetteRadiusLoc = glGetUniformLocation(shaderProgram, "vignetteRadius");
    glUniform1f(vignetteRadiusLoc, vignetteRadius);

    generateSphere(breatheRadius, smoothness, (smoothness / 2));

    // Update current actual radius
    radius = breatheRadius;

    
    // Update rim light pulsing and color
    rimStrength = rimPulser->evaluate(timeSeconds);

    animatedRimColor = glm::make_vec3(rimColor.data());
    if (rimAnimateEnabled) {
        float t = fmod(timeSeconds / rimAnimateFrequency, 1.0f);
        float r = 0.5f + 0.5f * sinf(2.0f * M_PI * t);
        float g = 0.5f + 0.5f * sinf(2.0f * M_PI * (t + 0.33f));
        float b = 0.5f + 0.5f * sinf(2.0f * M_PI * (t + 0.66f));
        animatedRimColor = glm::vec3(r, g, b);
    }
    
    
    // Update dust motes
    updateMotes();
}

void Florb::updateMotes() {
    // Update random walk for each dust mote
    for (auto i = 0UL; i < (2 * moteCount); ++i) {
        // float vecDir(2.0f * (dist(gen) - 0.5f));
        float vecDir(dist(gen));
        float step(moteMaxStep * vecDir);
        auto &component(moteCenters[i]);
        
        // Update this component by the computed step
        component += (step * moteDirections[i]);
        
        // Wrap the component to keep it on the sphere
        moteCenters[i] = fmod(moteCenters[i] + 1.0f, 2.0f) - 1.0f;
    }
}
