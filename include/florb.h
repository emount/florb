#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <GL/glew.h>

#include "flower.h"

// Florb class encapsulating the sphere state, geometry, textures, and behavior
class Florb {
private:
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

public:
    Florb();
    ~Florb();

    void loadFlowers(const std::string& directory);
    void nextFlower();
  
    float getZoom();
    void setZoom(float z);
  
    std::pair<float, float> getCenter();
    void setCenter(float x, float y);
  
    void renderFrame();

public:
  
    static const std::string k_DefaultImageDir;

private:
    void generateSphere();
    void initShaders();
};
