#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

class Spotlight {

public:

    // Constructor
    Spotlight(const std::string &name,
              const std::vector<float> &direction,
              float intensity,
              const std::vector<float> &color);

public:

    const std::vector<float>& getDirection() const;
    void setDirection(float alpha, float beta, float phi);

    float getIntensity() const;
    void setIntensity(float i);

    const std::vector<float>& getColor() const;
    void setColor(float r, float g, float b);
    
private:

    const std::string name;

    std::vector<float> direction;

    float intensity;

    std::vector<float> color;
      
    mutable std::mutex stateMutex;

};
