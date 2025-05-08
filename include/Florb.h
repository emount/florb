#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <mutex>
#include <string>
#include <vector>
#include <random>

#include "Flower.h"

// Florb class encapsulating the sphere state, geometry, textures, and behavior
class Florb {
public:
  
    // Enumerated type for render mode
    enum class RenderMode { FILL, LINE };

    // Enumerated type for specular mode
    enum class SpecularMode { NORMAL, DEBUG };

public:
    Florb();
    ~Florb();

    void nextFlower();

    void setTitle(const std::string &title);

    float getVideoFrameRate() const;
    void setVideoFrameRate(float r);

    float getImageSwitch() const;
    void setImageSwitch(float s);

    const std::vector<float>& getCameraView() const;
    void setCameraView(float alpha, float beta, float phi);

    std::pair<float, float> getCenter() const;
    void setCenter(float x, float y);
  
    float getZoom() const;
    void setZoom(float z);

    unsigned int getSmoothness() const;
    void setSmoothness(unsigned int s);

    const std::vector<float>& getLightDirection() const;
    void setLightDirection(float alpha, float beta, float phi);

    float getLightIntensity() const;
    void setLightIntensity(float i);

    float getShininess() const;
    void setShininess(float s);

    const std::vector<float>& getLightColor() const;
    void setLightColor(float r, float g, float b);
  
    float getVignetteRadius() const;
    void setVignetteRadius(float r);  

    float getVignetteExponent() const;
    void setVignetteExponent(float e);  
  
    const RenderMode& getRenderMode() const;
    void setRenderMode(const RenderMode &r);
  
    const SpecularMode& getSpecularMode() const;
    void setSpecularMode(const SpecularMode &s);
  
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

    std::vector<float> cameraView;
  
    std::vector<Flower> flowers;
    std::vector<std::string> flowerPaths;
    unsigned int currentFlower;

    float offsetX;
    float offsetY;
    float zoom;
    unsigned int smoothness;

    std::vector<float> lightDirection;
    float lightIntensity;
    float shininess;
    std::vector<float> lightColor;

    float vignetteRadius = 0.0f;
    float vignetteExponent = 0.0f;

    unsigned int moteCount;
    std::vector<float> moteRadii;
    std::vector<float> moteSpeeds;
    float moteMaxStep;
    std::vector<float> moteCenters;
    std::vector<float> moteDirections;
    std::vector<float> moteColor;

    RenderMode renderMode;
    SpecularMode specularMode;

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLuint shaderProgram = 0;
    GLuint fallbackTextureID = 0;
  
    int indexCount = 0;

    static const float k_MaxVideoFrameRate;
    static const float k_DefaultVideoFrameRate;
    static const float k_DefaultImageSwitch;

    static const float k_SphereRadius;
  
    static const int k_MaxMotes;

    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<float> dist;
  
    mutable std::mutex stateMutex;
  
};
