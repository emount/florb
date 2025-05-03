#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <GL/glew.h>

#include "flower.h"

// Florb class encapsulating the sphere state, geometry, textures, and behavior
class Florb {
public:
    Florb();
    ~Florb();

    void nextFlower();
  
    float getZoom();
    void setZoom(float z);
  
    std::pair<float, float> getCenter();
    void setCenter(float x, float y);
  
    void renderFrame();

private:
    void loadConfig();
    void loadFlowers();
    void generateSphere(float radius, int sectorCount, int stackCount);
    void initShaders();

public:
  
    static const std::string k_DefaultImagePath;
  
private:
    std::string imagePath;
  
    std::vector<Flower> flowers;
    std::vector<std::string> flowerPaths;
    unsigned int currentFlower = 0;

    float zoom = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLuint shaderProgram = 0;
    GLuint fallbackTextureID = 0;
  
    int indexCount = 0;
  
    std::mutex stateMutex;

    static const float k_SphereRadius;
    static const int k_SectorCount;
    static const int k_StackCount;
  
};
