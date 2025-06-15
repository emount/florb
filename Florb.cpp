#include <GL/glew.h>
#include <GL/glx.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdint.h>

#include "Camera.h"
#include "Florb.h"
#include "FlorbConfigs.h"
#include "FlorbUtils.h"
#include "SinusoidalMotion.h"
#include "Spotlight.h"


namespace chrono = std::chrono;
namespace fs = std::filesystem;

using chrono::duration_cast;
using chrono::milliseconds;
using chrono::steady_clock;

using std::cerr;
using std::cos;
using std::cout;
using std::default_random_engine;
using std::endl;
using std::make_shared;
using std::min;
using std::mt19937;
using std::pair;
using std::random_device;
using std::shared_ptr;
using std::shuffle;
using std::sin;
using std::sort;
using std::string;
using std::to_string;
using std::uniform_real_distribution;
using std::uint32_t;
using std::vector;


// Class Florb implementation

// Static attribute initialization

const float Florb::k_MaxMoteWinkTime(10.0f);

const float Florb::k_MinMoteWinkFrequency(0.2f);

const float Florb::k_MaxMoteWinkFrequency(1.0f);

const float Florb::k_MoteWinkThreshold(0.001f);


// Constructor

Florb::Florb() :
    flowers(),
    flowerPaths(),
    loadFlower(0UL),
    currentFlower(0UL),
    previousFlower(0UL),
    flowersRandom(),

    transitionStart(0.0f),

    cameras(),

    baseRadius(FlorbConfigs::k_DefaultRadius),
    
    baseRimStrength(0.0f),
    
    animatedRimColor(0.0f, 0.0f, 0.0f),
    
    moteCount(0UL),
    motesRadii(),
    motesSpeeds(),
    motesMaxStep(),
    motesCenters(),
    motesPulsers(),
    motesWinkingEnabled(),
    motesWinkTimes(),
    motesWinking(),
    motesAmplitudes(),
    motesMaxOff(),
    motesDirections(),
    motesColor(),

    configs(make_shared<FlorbConfigs>()),

    bouncer(make_shared<SinusoidalMotion>()),
    bounceOffset(0.0f),
    breather(make_shared<SinusoidalMotion>()),
    rimPulser(make_shared<SinusoidalMotion>()),

    loadingTexture(FlorbUtils::createTexture(0, 0, 0, 255)),
    fallbackTexture(FlorbUtils::createTexture(255, 0, 0, 255)),
    
    dist(0.0f, 1.0f) {

    // Load configs and initialize dependent elements
    configs->load();

    cameras = configs->getCameras();

    spotlights = configs->getSpotlights();

    createBouncer();
    
    baseRadius = configs->getRadius();

    baseRimStrength = configs->getRimStrength();
    
    createBreather();

    createRimPulser();
    
    initMotes(configs->getMoteCount(),
              configs->getMotesRadius(),
              configs->getMotesMaxStep(),
              configs->getMotesColor());

    unsigned seed(chrono::system_clock::now().time_since_epoch().count());
    flowersRandom = make_shared<default_random_engine>(seed);
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

shared_ptr<FlorbConfigs> Florb::getConfigs() const {
    return configs;
}

void Florb::nextFlower() {
    
    if (flowers.empty()) return;
    
    previousFlower = currentFlower;
    if(++currentFlower >= flowers.size()) {
        // Cache a reference to the last flower displayed
        const auto lastFlower(flowers.back());
        
        // Reset the current flower to the head of the collection
        currentFlower = 0;

        // If the transition order is random, re-shuffle the collection
        if (configs->getTransitionOrder() == FlorbConfigs::TransitionOrder::RANDOM) {
            bool shuffled(false);

            while(shuffled == false) {
                // Perform the shuffle for this iteration
                shuffle(flowers.begin(), flowers.end(), *flowersRandom);

                // If there is more than one flower, ensure that the last
                // flower is not the same as the next one
                if ((flowers.size() > 1) and (flowers.front() == lastFlower)) {
                    // Reshuffle to avoid redisplaying the same flower
                    shuffle(flowers.begin(), flowers.end(), *flowersRandom);
                } else shuffled = true;
            }
        }
    } // if (random transition order)
}


// Render frame method

void Florb::renderFrame(bool transition) {
    extern int screenWidth;
    extern int screenHeight;
    static bool firstFrame(true);
    
    FlorbUtils::glCheck("renderFrame()");

    if (!glXGetCurrentContext()) {
        cerr << "[renderFrame()] OpenGL context is not current" << endl;
    }

    // Perform one-time initialization upon the first frame
    if (firstFrame) {
        // Initialize the sphere
        auto smoothness(configs->getSmoothness());
        initSphere(smoothness, (smoothness / 2));

        // Begin loading flower images
        loadFlower = 0UL;
        if (flowers.empty() == false) {
            flowers[loadFlower++]->loadImage();
        }
    } else {
        // Load flower images one at a time
        if (loadFlower < flowers.size())
            flowers[loadFlower++]->loadImage();
    }

    // Update physical effects
    updatePhysicalEffects(transition);
    
    // Set a dark gray background color for the window
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // Set either line or fill rendering
    if (configs->getRenderMode() == FlorbConfigs::RenderMode::LINE) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    GLuint previousTexture;
    GLuint currentTexture;
    if(flowers.empty()) {
        // Display the fallback texture if there are no images
        previousTexture = currentTexture = fallbackTexture;
    } else if (loadFlower < flowers.size()) {
        // Suppress flower rendering until all images are loaded
        previousTexture = currentTexture = loadingTexture;
    } else {
        previousTexture = flowers[previousFlower]->getTextureID();
        currentTexture = flowers[currentFlower]->getTextureID();
    }
    
    if (!glIsTexture(currentTexture))
        cerr << "[WARN] Texture ID ("
             << currentTexture
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


    // Use texture units zero and one for current and previous flowers
    GLuint previousTextureLoc = glGetUniformLocation(shaderProgram, "previousTexture");
    GLuint currentTextureLoc = glGetUniformLocation(shaderProgram, "currentTexture");
    glUniform1i(previousTextureLoc, 0);
    glUniform1i(currentTextureLoc, 1);


    // Set screen resolution uniform
    GLuint resolutionLoc = glGetUniformLocation(shaderProgram, "resolution");
    glUniform2f(resolutionLoc, screenWidth, screenHeight);


    // Set screen aspect ratio uniform
    GLuint aspectRatioLoc = glGetUniformLocation(shaderProgram, "aspectRatio");
    glUniform1f(aspectRatioLoc, aspect);
    
    
    // Set offset and radius uniforms
    GLuint offsetLoc = glGetUniformLocation(shaderProgram, "offset");
    GLuint radiusLoc = glGetUniformLocation(shaderProgram, "radius");

    auto center(configs->getCenter());
    glUniform2f(offsetLoc, center.first, center.second);
    glUniform1f(radiusLoc, configs->getRadius());
    

    // Camera uniforms
    GLuint viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
    GLuint zoomLoc = glGetUniformLocation(shaderProgram, "zoom");
    
    const auto &cameraView(cameras[0]->getView());
    glUniform3f(viewPosLoc,
                cameraView[0],
                cameraView[1],
                cameraView[2]);

    glUniform1f(zoomLoc, cameras[0]->getZoom());

    
    // Set light uniforms
    int lightCount(spotlights.size());
    glUniform1i(glGetUniformLocation(shaderProgram, "lightCount"), lightCount);

    // Iterate through all spotlights
    for (int i = 0; i < lightCount; ++i) {
        const auto& spotlight = spotlights[i];
    
        string base = "spotlights[" + to_string(i) + "].";

        const auto &direction(spotlight->getDirection());
        
        float alpha = direction[0];
        float beta  = direction[1];

        glm::vec3 dir;
        dir.x = cos(alpha) * cos(beta);
        dir.y = sin(beta);
        dir.z = sin(alpha) * cos(beta);
        dir = glm::normalize(dir);
        
        const auto &spotlightColor(spotlight->getColor());
        glUniform3f(glGetUniformLocation(shaderProgram, (base + "direction").c_str()),
                    dir.x,
                    dir.y,
                    dir.z);
        glUniform3f(glGetUniformLocation(shaderProgram, (base + "color").c_str()),
                    spotlightColor[0],
                    spotlightColor[1],
                    spotlightColor[2]);
        glUniform1f(glGetUniformLocation(shaderProgram, (base + "intensity").c_str()),
                    spotlight->getIntensity());
    }

    
    // Set specular reflection uniforms
    GLuint shininessLoc = glGetUniformLocation(shaderProgram, "shininess");
    GLuint anisotropyEnabledLoc = glGetUniformLocation(shaderProgram, "anisotropyEnabled");
    GLuint anisotropyStrengthLoc = glGetUniformLocation(shaderProgram, "anisotropyStrength");
    GLuint anisotropySharpnessLoc = glGetUniformLocation(shaderProgram, "anisotropySharpness");
    glUniform1f(shininessLoc, configs->getShininess());
    glUniform1i(anisotropyEnabledLoc, configs->getAnisotropyEnabled());
    glUniform1f(anisotropyStrengthLoc, configs->getAnisotropyStrength());
    glUniform1f(anisotropySharpnessLoc, configs->getAnisotropySharpness());

    // Set debug uniforms
    GLuint anisotropicDebugLoc = glGetUniformLocation(shaderProgram, "anisotropicDebug");
    GLuint specularDebugLoc = glGetUniformLocation(shaderProgram, "specularDebug");
    
    int anisotropicDebug;
    if (configs->getAnisotropicMode() == FlorbConfigs::AnisotropicMode::NORMAL) {
        anisotropicDebug = 0;
    } else {
        anisotropicDebug = 1;
    }
    glUniform1i(anisotropicDebugLoc, anisotropicDebug);
    
    int specularDebug;
    if (configs->getSpecularMode() == FlorbConfigs::SpecularMode::NORMAL) {
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
    glUniform1f(rimExponentLoc, configs->getRimExponent());
    glUniform1f(rimStrengthLoc, configs->getRimStrength());


    // Set vignette uniforms
    GLuint vignetteRadiusLoc = glGetUniformLocation(shaderProgram, "vignetteRadius");
    GLuint vignetteExponentLoc = glGetUniformLocation(shaderProgram, "vignetteExponent");
    glUniform1f(vignetteRadiusLoc, configs->getVignetteRadius());
    glUniform1f(vignetteExponentLoc, configs->getVignetteExponent());


    // Set iridescence uniforms
    GLuint iridescenceStrengthLoc = glGetUniformLocation(shaderProgram, "iridescenceStrength");
    GLuint iridescenceFrequencyLoc = glGetUniformLocation(shaderProgram, "iridescenceFrequency");
    GLuint iridescenceShiftLoc = glGetUniformLocation(shaderProgram, "iridescenceShift");
    glUniform1f(iridescenceStrengthLoc, configs->getIridescenceStrength());
    glUniform1f(iridescenceFrequencyLoc, configs->getIridescenceFrequency());
    glUniform1f(iridescenceShiftLoc, configs->getIridescenceShift());

    
    // Dust mote uniforms
    GLuint moteCountLoc = glGetUniformLocation(shaderProgram, "moteCount");
    GLuint motesRadiiLoc = glGetUniformLocation(shaderProgram, "motesRadii");
    GLuint motesSpeedsLoc = glGetUniformLocation(shaderProgram, "motesSpeeds");
    GLuint motesCentersLoc = glGetUniformLocation(shaderProgram, "motesCenters");
    GLuint motesAmplitudesLoc = glGetUniformLocation(shaderProgram, "motesAmplitudes");
    GLuint motesColorLoc = glGetUniformLocation(shaderProgram, "motesColor");
    glUniform1i(moteCountLoc, moteCount);
    glUniform1fv(motesRadiiLoc, motesRadii.size(), motesRadii.data());
    glUniform1fv(motesSpeedsLoc, motesSpeeds.size(), motesSpeeds.data());
    glUniform2fv(motesCentersLoc, motesCenters.size(), motesCenters.data());
    glUniform1fv(motesAmplitudesLoc, motesAmplitudes.size(), motesAmplitudes.data());
    glUniform3f(motesColorLoc, motesColor[0], motesColor[1], motesColor[2]);


    // Flutter wave uniforms
    GLuint waveAmplitudeLoc = glGetUniformLocation(shaderProgram, "waveAmplitude");
    GLuint waveFrequencyLoc = glGetUniformLocation(shaderProgram, "waveFrequency");
    GLuint waveSpeedLoc = glGetUniformLocation(shaderProgram, "waveSpeed");
    glUniform1f(waveAmplitudeLoc, configs->getFlutterAmplitude());
    glUniform1f(waveFrequencyLoc, configs->getFlutterFrequency());
    glUniform1f(waveSpeedLoc, configs->getFlutterSpeed());


    // Bounce offset uniform
    GLuint bounceOffsetLoc = glGetUniformLocation(shaderProgram, "bounceOffset");
    glUniform1f(bounceOffsetLoc, bounceOffset);

    
    // Activate textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, previousTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, currentTexture);

    
    glBindVertexArray(vao);
    FlorbUtils::glCheck("glBindVertexArray(vao)");

    glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
    FlorbUtils::glCheck("glDrawElements()");
    
    glBindVertexArray(0);
    FlorbUtils::glCheck("glBindVertexArray(0) - A");

    // Flush the OpenGL pipeline
    glFlush();

    firstFrame = false;
}


// Private methods

// Load flowers method

void Florb::loadFlowers() {
    const auto &imagePaths(configs->getImagePaths());

    // Iterate through the image paths, precomputing the total number of flower images
    uint32_t numFlowers(0UL);
    for (const auto &imagePath : imagePaths) {
        fs::path filepath(imagePath);
        
        if(fs::is_directory(filepath))
            for (const auto& entry : fs::directory_iterator(imagePath))
                if (entry.is_regular_file()) numFlowers++;
    }                

    // Re-iterate through the image paths to actually load flower images
    float percentLoaded(0.0f);
    for (const auto &imagePath : imagePaths) {
        fs::path filepath(imagePath);
        
        if(fs::is_directory(filepath)) {
            // Iterate over the filenames, constructing Flowers
            for (const auto& entry : fs::directory_iterator(imagePath)) {
                if (entry.is_regular_file()) {
                    // Push the flower image onto the back of the collection
                    flowers.push_back(make_shared<Flower>(entry.path().string()));

                    // Update progress bar uniforms
                    percentLoaded = (static_cast<float>(flowers.size()) / numFlowers);
                    // bool showBar(percentLoaded >= 1.0f);
                    // glUniform1f(glGetUniformLocation(shaderProgram, "loadProgress"), percentLoaded);
                    // glUniform1i(glGetUniformLocation(shaderProgram, "showProgressBar"), showBar ? 1 : 0);
                }
            }
        
            // Determine the ordering mode to use for the Flowers
            if (configs->getTransitionOrder() == FlorbConfigs::TransitionOrder::ALPHABETICAL) {
                // Sort the collection of Flowers by filename
                sort(flowers.begin(),
                     flowers.end(),
                     [](const shared_ptr<Flower>& a, const shared_ptr<Flower>& b) {
                         return a->getFilename() < b->getFilename();
                     });
            } else {
                // Randomly shuffle the collection of Flowers
                shuffle(flowers.begin(), flowers.end(), *flowersRandom);
            }
        } else {
            cerr << "Image path \"" << imagePath << "\" does not exist" << endl;
        }
    }
}


// Flower transition update method

void Florb::updateTransition(bool transition, float timeSeconds) {
    auto transitionMode(configs->getTransitionMode());

    if (transition) transitionStart = timeSeconds;

    float progress;
    if (transitionMode == FlorbConfigs::TransitionMode::BLEND) {
        // Compute transition progress
        progress = ((timeSeconds - transitionStart) / configs->getTransitionTime());
    } else {
        // Progress completes immediately
        progress = 1.0f;
    }
        
    GLuint transitionProgressLoc = glGetUniformLocation(shaderProgram, "transitionProgress");
    glUniform1f(transitionProgressLoc, progress);
}


// Sphere geometry methods

void Florb::initSphere(int sectorCount, int stackCount) {
    // Generate and bind the vertex array
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Allocate vertex buffer
    int numVertices((stackCount + 1) * (sectorCount + 1));
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, numVertices * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);
    FlorbUtils::glCheck("glBufferData(GL_ARRAY_BUFFER)");

    // Allocate index buffer
    int numIndices(stackCount * (sectorCount + 1) * 2);
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
    FlorbUtils::glCheck("glBufferData(GL_ELEMENT_ARRAY_BUFFER)");
}

void Florb::generateSphere(float radius, int sectorCount, int stackCount) {
    vector<Vertex> vertices;
    vector<unsigned int> indices;
    
    for (int y = 0; y <= stackCount; ++y) {
        for (int x = 0; x <= sectorCount; ++x) {
            float xSegment = static_cast<float>(x) / sectorCount;
            float ySegment = static_cast<float>(y) / stackCount;
            float xPos = radius * cos(xSegment * 2.0f * M_PI) * sin(ySegment * M_PI);
            float yPos = radius * cos(ySegment * M_PI);
            float zPos = radius * sin(xSegment * 2.0f * M_PI) * sin(ySegment * M_PI);

            Vertex v;
            v.position = glm::vec3(xPos, yPos, zPos);
            v.normal = glm::normalize(v.position);
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

    // Bind vertex and index arrays
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    // Load vertices into existing buffer
    glBufferSubData(GL_ARRAY_BUFFER,
                    0,
                    vertices.size() * sizeof(Vertex),
                    vertices.data());
    FlorbUtils::glCheck("glBufferSubData(GL_ARRAY_BUFFER)");

    // Load indices into existing buffer
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                    0,
                    indices.size() * sizeof(GLuint),
                    indices.data());
    FlorbUtils::glCheck("glBufferSubData(GL_ELEMENT_ARRAY_BUFFER)");

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

        #define PI 3.141592654

        uniform float time;

        uniform vec2 resolution;
        uniform float aspectRatio;

        uniform vec2 offset;
        uniform float zoom;
        uniform float radius;

        uniform int anisotropyEnabled;
        uniform float anisotropyStrength;
        uniform float anisotropySharpness;

        #define MAX_MOTES 256
        uniform int moteCount;
        uniform float motesRadii[MAX_MOTES];
        uniform float motesSpeeds[MAX_MOTES];
        uniform vec2 motesCenters[MAX_MOTES];
        uniform float motesAmplitudes[MAX_MOTES];
        uniform vec3 motesColor;

        uniform vec3 rimColor;
        uniform float rimExponent;
        uniform float rimStrength;
        
        uniform float vignetteRadius;
        uniform float vignetteExponent;

        uniform float iridescenceStrength;
        uniform float iridescenceFrequency;
        uniform float iridescenceShift;

        #define MAX_LIGHTS 4
        
        struct Spotlight {
            vec3 direction;
            vec3 color;
            float intensity;
        };
        
        uniform Spotlight spotlights[MAX_LIGHTS];
        uniform int lightCount;

        uniform vec3 viewPos;
        uniform float shininess;

        uniform float waveAmplitude;
        uniform float waveFrequency;
        uniform float waveSpeed;

        uniform sampler2D currentTexture;
        uniform sampler2D previousTexture;
        uniform float transitionProgress;

        uniform int anisotropicDebug;
        uniform int specularDebug;

        // uniform float loadProgress;
        // uniform int showProgressBar;
        #define loadProgress 0.5
        #define showProgressBar 1
        
        void drawStatusBar(inout vec4 FragColor, vec2 screenUV, vec2 resolution) {
            if ((showProgressBar == 0) || loadProgress >= 1.0) return;
            
            // Bar dimensions (relative)
            float barWidth = 0.90;
            float barHeight = 0.03;
            float radius = 0.015;
            
            // Vertically centered
            vec2 center = vec2(0.5, 0.5);
            vec2 halfSize = vec2(barWidth * 0.5, barHeight * 0.5);
            vec2 uv = screenUV;
            
            // Relative to bar center
            vec2 d = abs(uv - center) - halfSize;
            
            // Rounded rectangle mask
            float dist = length(max(d, 0.0)) - radius;
            float alpha = smoothstep(0.005, 0.0, dist);
            
            // Progress mask
            float progressRight = center.x - halfSize.x + barWidth * loadProgress;
            float inProgress = step(uv.x, progressRight);
            
            vec3 barColor = vec3(0.0, 0.8, 0.0); // Solid green
            
            // Composite blend
            FragColor.rgb = mix(FragColor.rgb, barColor, alpha * inProgress);
        }

        
        void main() {

            // Obtain a normalized direction vector from the fragment shader
            vec3 dir = normalize(fragPos);

            // Use spherical coordinates to modulate wave phase
            float wavePhase =
                ((dot(dir, vec3(1.0, 0.0, 0.0)) * waveFrequency) - (time * waveSpeed));

            // Sine-based displacement along a direction (e.g. vertical in UV space)
            vec2 waveOffset = vec2(0.0, sin(wavePhase) * waveAmplitude);

            // Convert fragment direction to spherical UV coordinates
            vec2 uv;
            uv.x = atan(dir.z, dir.x) / (2.0 * PI) + 0.5;
            uv.y = asin(dir.y) / PI + 0.5;

            // Apply zoom and offset after spherical conersion, flipping vertically
            uv = (uv - 0.5) * zoom + 0.5 + offset;
            uv.y = 1.0 - uv.y;

            // Apply aspect ratio correction centered on (0.5, 0.5)
            uv.x = (uv.x - 0.5) * aspectRatio + 0.5;
            
            // Clamp to avoid oversampling outside the texture
            uv = clamp(uv, vec2(0.0), vec2(1.0));

            // Apply displacement to UVs before texture sampling
            uv += waveOffset;
            uv = clamp(uv, vec2(0.0), vec2(1.0));

            // Discard any pixel locations beyond the radius of the sphere
            if (length(fragPos) > radius)
                discard;


            // Dust mote contributions
            float moteGlow = 0.0;
            float dust = 0.0;
            for (int i = 0; i < moteCount; ++i) {
                float speed = motesSpeeds[i];
                float radius = motesRadii[i] / resolution.y;
            
                // Wobbling orbit using sin/cos with time
                float wobbleX = sin(time * speed);
                float wobbleY = cos(time * speed * 0.5);
            
                vec2 orbitOffset = vec2(wobbleX, wobbleY) * 0.01;

                // Map [-1,1] to [0,1]
                vec2 motePos = (motesCenters[i] * 0.5 + 0.5);
                motePos += orbitOffset;
            
                float dist = distance(uv, motePos);
                float alpha = smoothstep(radius, 0.0, dist);

                dust += motesAmplitudes[i] * alpha;
            }

            // Aspect-corrected center-relative coords
            vec2 screenUV = gl_FragCoord.xy / resolution;
            vec2 centered = screenUV - vec2(0.5);
            centered.x *= resolution.x / resolution.y;


            // Diffuse lighting
            vec3 norm = normalize(fragNormal);


            // Spotlighting
            vec3 totalLighting = vec3(0.0);
            vec3 viewDir = normalize(viewPos - fragPos);
            float totalSpecular = 0.0;
            vec3 anisotropicColor = vec3(0.0);
            for (int i = 0; i < lightCount; ++i) {
                vec3 lightDir = normalize(-spotlights[i].direction);
                vec3 reflectDir = reflect(-lightDir, norm);
            
                float diff = max(dot(norm, lightDir), 0.0);
                float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
                totalSpecular += spec;
            
                vec3 lightColor = spotlights[i].color * spotlights[i].intensity;
                vec3 lighting = (diff + spec) * lightColor;

                anisotropicColor += lightColor;
            
                totalLighting += lighting;
            }

            // Average the spotlight colors to produce the anisotropic color
            if (lightCount > 0) anisotropicColor /= lightCount;


            // Anisotropic specular reflections
            vec3 anisotropicLightDir = -spotlights[0].direction;
            vec3 anisotropicN = normalize(fragNormal);
            vec3 anisotropicT = normalize(dFdx(fragPos));
            vec3 anisotropicB = normalize(dFdy(fragPos));
            vec3 tangent = normalize(anisotropicT - norm * dot(norm, anisotropicT));
            vec3 bitangent = cross(norm, tangent);
            
            // Simulate using a modified Blinn-Phong calculation
            vec3 anisotropicH = normalize(viewDir + anisotropicLightDir);
            float dotTH = dot(tangent, anisotropicH);
            float dotNH = dot(norm, anisotropicH);
            
            float th2 = (dotTH * dotTH);
            float anisotropicFactor = pow(max(th2, 0.0001), anisotropySharpness);
            float anisotropicSpec = pow(max(dotNH, 0.0), shininess) * anisotropicFactor;
            
            vec3 specularColor = (anisotropicColor * pow(max(dotNH, 0.0), shininess) *
                                  anisotropicFactor * anisotropyStrength * 10.0);


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


            // Iridescence effect
            vec3 iridescenceN = normalize(fragNormal);
            vec3 iridescenceV = normalize(viewPos - fragPos);
            float angle = dot(iridescenceN, iridescenceV);

            float facing = clamp(1.0 - angle, 0.0, 1.0);
            float iridescence = sin(facing * iridescenceFrequency + iridescenceShift);
            iridescence = 0.5 + 0.5 * iridescence;

            vec3 shimmerColor = vec3(
                0.5 + 0.5 * sin(6.2831 * iridescence + 0.0),
                0.5 + 0.5 * sin(6.2831 * iridescence + 2.0),
                0.5 + 0.5 * sin(6.2831 * iridescence + 4.0)
            );


            // Sample texture colors
            vec4 colorPrev = texture(previousTexture, uv);
            vec4 colorCurr = texture(currentTexture, uv);
            
            vec4 texColor = mix(colorPrev, colorCurr, clamp(transitionProgress, 0.0, 1.0));

            // Compute final color
            vec3 finalColor = vignette * totalLighting * texColor.rgb;

            if (anisotropyEnabled == 1) finalColor += specularColor;

            finalColor += rimLight;

            // Clamp and overlay colored dust mote glow
            finalColor += clamp(dust, 0.0, 1.0) * motesColor;

            // Incorporate iridescence
            finalColor.rgb = mix(finalColor.rgb, shimmerColor, iridescenceStrength);


            // Assign final color, taking debug modes into account
            if (anisotropicDebug == 1) {
               FragColor = vec4(specularColor, 1.0);
            } else if (specularDebug == 1) {
               FragColor = vec4(vec3(totalSpecular), 1.0);
            } else {
                FragColor = vec4(finalColor, 1.0);
            }

            // Update the status bar
            drawStatusBar(FragColor, screenUV, resolution);
        }
    )glsl";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    if (vertexShader == 0) {
        cerr << "glCreateShader(GL_VERTEX_SHADER) failed — returned 0 (invalid handle)" << endl;
    }
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    GLint vertexStatus = 0;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vertexStatus);
    if (vertexStatus != GL_TRUE) {
        char infoLog[2048];
        glGetShaderInfoLog(vertexShader, sizeof(infoLog), nullptr, infoLog);
        cerr << "[Vertex Shader Compile Error]\n" << infoLog << endl;
    }
    FlorbUtils::glCheck("glCompileShader(vertexShader)");


    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    if (fragmentShader == 0) {
        cerr << "glCreateShader(GL_FRAGMENT_SHADER) failed — returned 0 (invalid handle)" << endl;
    }
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    GLint fragmentStatus = 0;
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fragmentStatus);
    if (fragmentStatus != GL_TRUE) {
        char infoLog[2048];
        glGetShaderInfoLog(fragmentShader, sizeof(infoLog), nullptr, infoLog);
        cerr << "[Fragment Shader Compile Error]\n" << infoLog << endl;
    }
    FlorbUtils::glCheck("glCompileShader(fragmentShader)");

    
    // Create and link the full program from its constituents
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    GLint linkStatus = 0;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
        char log[2048];
        glGetProgramInfoLog(shaderProgram, sizeof(log), nullptr, log);
        cerr << "Shader Link Error:\n" << log << endl;
    }
    FlorbUtils::glCheck("glLinkProgram()");


    // Free the memory used by both shader programs
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}


// Dust mote methods

void Florb::initMotes(unsigned int count,
                      float radius,
                      float maxStep,
                      const vector<float> &color) {
    // Randomize dust mote centers
    moteCount = count;
    motesCenters.resize(2 * moteCount);
    motesPulsers.resize(moteCount);
    motesWinking.resize(moteCount);
    motesWinkTimes.resize(moteCount);
    for (auto i = 0UL; i < (2 * moteCount); i++) {
        // Sample in [-1.0, 1.0] to cover full UV space after offset/zoom
        motesCenters[i] = ((2.0f * dist(gen)) - 1.0f);
        
        if ((i % 2) == 0) {
            float bias(dist(gen));
            if (bias > 0.5f) {
                motesDirections.push_back(1.0f);
            } else {
                motesDirections.push_back(-1.0f);
            }
        } else motesDirections.push_back(1.0f);
    }

    for (auto i = 0UL; i < moteCount; i++) {
        auto winkAmplitude((k_MaxMoteWinkFrequency - k_MinMoteWinkFrequency) *
                           dist(gen) / 2.0f);
        auto winkFrequency(k_MinMoteWinkFrequency + winkAmplitude);
        
        motesPulsers[i] =
            make_shared<SinusoidalMotion>(true, 0.5f, 0.5f, winkFrequency, 0.0f);

        motesWinking[i] = ((dist(gen) > 0.5) ? true : false);
        motesWinkTimes[i] = (k_MaxMoteWinkTime * dist(gen));
    }    
    
    motesRadii = vector<float>(moteCount, radius);
    motesSpeeds = vector<float>(moteCount, maxStep);
    motesMaxStep = maxStep;
    motesAmplitudes = vector<float>(moteCount, 0.0f);
    motesColor = color;
}

void Florb::updateMotes(float timeSeconds) {
    // Update random walk for each dust mote
    for (auto i = 0UL; i < (2 * moteCount); i++) {
        float vecDir(dist(gen));
        float step(motesMaxStep * vecDir);
        auto &component(motesCenters[i]);
        
        // Update this component by the computed step
        component += (step * motesDirections[i]);
        
        // Wrap the component to keep it on the sphere
        motesCenters[i] = fmod(motesCenters[i] + 1.0f, 2.0f) - 1.0f;
    }

    // Update mote winking state and amplitudes for winking firefly effect
    for (auto i = 0UL; i < moteCount; i++) {
        if (motesWinking[i] == true) {           
            motesAmplitudes[i] = motesPulsers[i]->evaluate(timeSeconds);

            if (motesAmplitudes[i] <= k_MoteWinkThreshold) {
                motesWinkTimes[i] = (timeSeconds + (k_MaxMoteWinkTime * dist(gen)));
                motesWinking[i] = false;
            }
        } else if (timeSeconds >= motesWinkTimes[i]) {
            motesWinking[i] = true;
        }
    }
}


// Physical effects methods

void Florb::createBouncer() {
    float phase(0.0f);

    bool enabled(configs->getBounceEnabled());
    float bias(enabled ? configs->getBounceAmplitude() : 0.0f);
    float amplitude(bias);
    bouncer = make_shared<SinusoidalMotion>(
        enabled,
        bias,
        amplitude,
        configs->getBounceFrequency(),
        phase
    );
}

void Florb::createBreather() {
    float phase(M_PI / 2);

    const auto &breatheAmplitude(configs->getBreatheAmplitude());
    float minR = breatheAmplitude[0];
    float maxR = breatheAmplitude[1];
    float bias = 0.5f * (minR + maxR);
    float amplitude = 0.5f * (maxR - minR);

    breather = make_shared<SinusoidalMotion>(
        configs->getBreatheEnabled(),
        bias,
        amplitude,
        configs->getBreatheFrequency(),
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
        configs->getRimFrequency(),
        phase
    );
}


// Update physical effects method

void Florb::updatePhysicalEffects(bool transition) {
    // Update the time uniform for physical effects
    static steady_clock::time_point startTime = steady_clock::now();
    float timeMsec = duration_cast<milliseconds>(steady_clock::now() - startTime).count();
    float timeSeconds = (timeMsec / 1000.0f);

    GLuint timeLoc = glGetUniformLocation(shaderProgram, "time");
    glUniform1f(timeLoc, timeSeconds);


    // Update flower image transition progress
    updateTransition(transition, timeSeconds);


    // Update spotlight motion
    for (auto &spotlight : spotlights) spotlight->updateMotion(timeSeconds);


    // Update the sphere
    bounceOffset = bouncer->evaluate(timeSeconds);
    auto breatheRadius(breather->evaluate(timeSeconds));
    configs->setVignetteRadius(breatheRadius);

    GLuint vignetteRadiusLoc = glGetUniformLocation(shaderProgram, "vignetteRadius");
    glUniform1f(vignetteRadiusLoc, vignetteRadius);

    auto smoothness(configs->getSmoothness());
    generateSphere(breatheRadius, smoothness, (smoothness / 2));

    // Update current actual radius
    configs->setRadius(breatheRadius);

    
    // Update rim light pulsing and color
    configs->setRimStrength(rimPulser->evaluate(timeSeconds));

    animatedRimColor = glm::make_vec3(configs->getRimColor().data());
    if (configs->getRimAnimateEnabled()) {
        float t = fmod(timeSeconds / configs->getRimAnimateFrequency(), 1.0f);
        float r = 0.5f + 0.5f * sinf(2.0f * M_PI * t);
        float g = 0.5f + 0.5f * sinf(2.0f * M_PI * (t + 0.33f));
        float b = 0.5f + 0.5f * sinf(2.0f * M_PI * (t + 0.66f));
        animatedRimColor = glm::vec3(r, g, b);
    }


    // Update dust motes
    updateMotes(timeSeconds);
}
