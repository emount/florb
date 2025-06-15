#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>
#include <random>

#include "Flower.h"

// Class forward references
class Camera;
class FlorbConfigs;
class MotionAlgorithm;
class Spotlight;


// Florb class encapsulating the sphere state, geometry, textures, and behavior
class Florb {

public:
    Florb();
    virtual ~Florb();

    std::shared_ptr<FlorbConfigs> getConfigs() const;

    void nextFlower();
  
    void renderFrame(bool transition);

private:
  
    void loadFlowers();

    void updateTransition(bool transition, float timeSeconds);

    void initSphere(int sectorCount, int stackCount);
    void generateSphere(float radius, int sectorCount, int stackCount);
  
    void initShaders();
  
    void initMotes(unsigned int count,
		   float radius,
		   float maxStep,
		   const std::vector<float> &color);
    void updateMotes(float timeSeconds);

    void createBouncer();
    void createBreather();
    void createRimPulser();
  
    void updatePhysicalEffects(bool transition);

private:

    // Structure for passing vertices to the vertex shader
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
    };
  
private:
  
    std::vector<std::shared_ptr<Flower>> flowers;
    std::vector<std::string> flowerPaths;
    unsigned int loadFlower;
    unsigned int currentFlower;
    unsigned int previousFlower;
    std::shared_ptr<std::default_random_engine> flowersRandom;

    float transitionStart;

    std::vector<std::shared_ptr<Camera>> cameras;

    std::vector<std::shared_ptr<Spotlight>> spotlights;  

    float baseRadius;

    float baseRimStrength;

    glm::vec3 animatedRimColor;

    float vignetteRadius = 0.0f;
    float vignetteExponent = 0.0f;

    unsigned int moteCount;
    std::vector<float> motesRadii;
    std::vector<float> motesSpeeds;
    float motesMaxStep;
    std::vector<float> motesCenters;
    std::vector<std::shared_ptr<MotionAlgorithm>> motesPulsers;
    std::vector<bool> motesWinkingEnabled;
    std::vector<float> motesWinkTimes;
    std::vector<bool> motesWinking;
    std::vector<float> motesAmplitudes;
    std::vector<float> motesMaxOff;
    std::vector<float> motesDirections;
    std::vector<float> motesColor;

    std::shared_ptr<FlorbConfigs> configs;

    std::shared_ptr<MotionAlgorithm> bouncer;
    float bounceOffset;
    std::shared_ptr<MotionAlgorithm> breather;
    std::shared_ptr<MotionAlgorithm> rimPulser;

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLuint shaderProgram = 0;
    GLuint loadingTexture = 0;
    GLuint fallbackTexture = 0;
  
    int indexCount = 0;

    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<float> dist;

    static const float k_MaxMoteWinkTime;
    static const float k_MinMoteWinkFrequency;
    static const float k_MaxMoteWinkFrequency;
    static const float k_MoteWinkThreshold;
  
};
