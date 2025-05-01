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
    int currentFlower = 0;

    float zoom = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLuint shaderProgram = 0;
    int indexCount = 0;

    std::mutex stateMutex;

public:
    Florb();
    ~Florb();

    void loadFlowers(const std::string& directory);
    void setZoom(float z);
    void setCenter(float x, float y);
    float getZoom();
    std::pair<float, float> getCenter();
    void renderFrame();

private:
    void createSphereGeometry();
    void initShader();
};
