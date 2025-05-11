#include <GL/glew.h>
#include <GL/glx.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>

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
using std::endl;
using std::make_shared;
using std::min;
using std::mt19937;
using std::pair;
using std::random_device;
using std::shared_ptr;
using std::sin;
using std::sort;
using std::string;
using std::to_string;
using std::uniform_real_distribution;
using std::vector;


// Florb class implementation

// Constructor

Florb::Florb() :
    flowers(),
    flowerPaths(),
    currentFlower(0UL),

    cameras(),

    baseRadius(FlorbConfigs::k_DefaultRadius),
    
    baseRimStrength(0.0f),
    
    animatedRimColor(0.0f, 0.0f, 0.0f),
    
    moteCount(0UL),
    moteRadii(),
    moteSpeeds(),
    moteMaxStep(),
    moteCenters(),
    moteDirections(),
    moteColor(3, 0.0f),

    configs(make_shared<FlorbConfigs>()),

    bouncer(make_shared<SinusoidalMotion>()),
    bounceOffset(0.0f),
    breather(make_shared<SinusoidalMotion>()),
    rimPulser(make_shared<SinusoidalMotion>()),
    
    fallbackTextureID(FlorbUtils::createDebugTexture()),
    
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
              configs->getMoteRadius(),
              configs->getMoteMaxStep(),
              configs->getMoteColor());

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
    if(++currentFlower >= flowers.size()) currentFlower = 0;
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

    // Perform one-time initialization upon the first frame
    if (firstFrame) {
        // Initialize the sphere
        auto smoothness(configs->getSmoothness());
        initSphere(smoothness, (smoothness / 2));
      
        // Load all flower images
        for (auto &flower : flowers) {
            flower->loadImage();
        }
    }

    // Update physical effects
    updatePhysicalEffects();
    
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
    
    GLuint textureID;
    if(flowers.empty()) {
        textureID = fallbackTextureID;
    } else {
        auto flower = flowers[currentFlower];
        textureID = flower->getTextureID();
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
        const auto& light = spotlights[i];
    
        string base = "spotlights[" + to_string(i) + "].";
        
        glUniform3f(glGetUniformLocation(shaderProgram, (base + "direction").c_str()),
                    light->getDirection()[0],
                    light->getDirection()[1],
                    light->getDirection()[2]);
        glUniform3f(glGetUniformLocation(shaderProgram, (base + "color").c_str()),
                    light->getColor()[0],
                    light->getColor()[1],
                    light->getColor()[2]);
        glUniform1f(glGetUniformLocation(shaderProgram, (base + "intensity").c_str()),
                    light->getIntensity());
    }

    
    // Set specular reflection uniforms
    GLuint shininessLoc = glGetUniformLocation(shaderProgram, "shininess");
    glUniform1f(shininessLoc, configs->getShininess());

    // Render either normally or in specular debug mode
    GLuint specularDebugLoc = glGetUniformLocation(shaderProgram, "specularDebug");
    
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


    // Set vignette effect uniforms
    GLuint vignetteRadiusLoc = glGetUniformLocation(shaderProgram, "vignetteRadius");
    GLuint vignetteExponentLoc = glGetUniformLocation(shaderProgram, "vignetteExponent");
    glUniform1f(vignetteRadiusLoc, configs->getVignetteRadius());
    glUniform1f(vignetteExponentLoc, configs->getVignetteExponent());

    
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

    // Flush the OpenGL pipeline
    glFlush();

    firstFrame = false;
}


// Private methods

void Florb::loadFlowers() {
    const auto &imagePath(configs->getImagePath());
    fs::path filepath(imagePath);
    
    if(fs::is_directory(filepath)) {
        // Iterate over the filenames, constructing Flowers
        for (const auto& entry : fs::directory_iterator(imagePath)) {
            if (entry.is_regular_file()) {
                flowers.push_back(make_shared<Flower>(entry.path().string()));
            }
        }

        // Sort the collection of Flowers by filename
        sort(flowers.begin(),
             flowers.end(),
             [](const shared_ptr<Flower>& a, const shared_ptr<Flower>& b) {
                 return a->getFilename() < b->getFilename();
             });
    } else {
        cerr << "Image path \"" << imagePath << "\" does not exist" << endl;
    }
}

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


            // Spotlighting
            vec3 totalLighting = vec3(0.0);
            vec3 viewDir = normalize(viewPos - fragPos);
            float totalSpecular = 0.0;
            for (int i = 0; i < lightCount; ++i) {
                vec3 lightDir = normalize(-spotlights[i].direction);
                vec3 reflectDir = reflect(-lightDir, norm);
            
                float diff = max(dot(norm, lightDir), 0.0);
                float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
                totalSpecular += spec;
            
                vec3 lightColor = spotlights[i].color * spotlights[i].intensity;
                vec3 lighting = (diff + spec) * lightColor;
            
                totalLighting += lighting;
            }


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

            // Compute final color
            vec3 finalColor = vignette * totalLighting * texColor.rgb;
            finalColor += rimLight;

            // Clamp and overlay colored dust mote glow
            finalColor += clamp(dust, 0.0, 1.0) * moteColor;

            // Assign final color, taking debug modes into account
            if (specularDebug != 0) {
               FragColor = vec4(vec3(totalSpecular), 1.0);
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
