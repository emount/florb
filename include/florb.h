#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <mutex>
#include <string>
#include <vector>
#include <random>

#include "flower.h"

// Florb class encapsulating the sphere state, geometry, textures, and behavior
class Florb {
public:
  
    // Enumerated type for render mode
    enum class RenderMode { FILL, LINE };

public:
    Florb();
    ~Florb();

    void nextFlower();

    void setTitle(const std::string &title);

    float getVideoFrameRate() const;
    void setVideoFrameRate(float r);

    float getImageSwitch() const;
    void setImageSwitch(float s);

    std::pair<float, float> getCenter() const;
    void setCenter(float x, float y);
  
    float getZoom() const;
    void setZoom(float z);

    const std::vector<float>& getLightDirection() const;
    void setLightDirection(float alpha, float beta, float phi);

    float getLightIntensity() const;
    void setLightIntensity(float i);

    const std::vector<float>& getLightColor() const;
    void setLightColor(float r, float g, float b);
  
    float getVignetteRadius() const;
    void setVignetteRadius(float r);  

    float getVignetteExponent() const;
    void setVignetteExponent(float e);  
  
    const RenderMode& getRenderMode() const;
    void setRenderMode(const RenderMode &r);
  
    void renderFrame();

private:
    void loadConfigs();
    void loadFlowers();
    void generateSphere(float radius, int sectorCount, int stackCount);
    void initShaders();
    void initMotes(unsigned int count,
		   float radius,
		   float maxStep,
		   const std::vector<float> &color);
    void updateMotes();

public:

    static const std::string k_DefaultTitle;
  
    static const std::string k_DefaultImagePath;

private:

    // Structure for passing vertices to the vertex shader
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
    };
  
private:
    std::string imagePath;

    float videoFrameRate;
    float imageSwitch;
  
    std::vector<Flower> flowers;
    std::vector<std::string> flowerPaths;
    unsigned int currentFlower;

    float zoom = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    std::vector<float> lightDirection;
    float lightIntensity;
    std::vector<float> lightColor;

    float vignetteRadius = 0.0f;
    float vignetteExponent = 0.0f;

    unsigned int moteCount;
    std::vector<float> moteRadii;
    std::vector<float> moteSpeeds;
    std::vector<float> moteCenters;
    std::vector<float> moteColor;

    RenderMode renderMode;

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLuint shaderProgram = 0;
    GLuint fallbackTextureID = 0;
  
    int indexCount = 0;
  
    mutable std::mutex stateMutex;

    static const float k_MaxVideoFrameRate;
    static const float k_DefaultVideoFrameRate;
    static const float k_DefaultImageSwitch;

    static const float k_SphereRadius;
  
    static const int k_SectorCount;
    static const int k_StackCount;

    static const int k_XSegments;
    static const int k_YSegments;

    static const int k_MaxMotes;

    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<float> dist;
  
};
