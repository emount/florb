#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

class Camera {

public:

    // Constructor
    Camera(const std::string &name,
           const std::vector<float> &view,
           float zoom);

public:

    const std::vector<float>& getView() const;
    void setView(float alpha, float beta, float phi);

    float getZoom() const;
    void setZoom(float z);
    
private:

    const std::string name;

    std::vector<float> view;
    
    float zoom;
      
    mutable std::mutex stateMutex;

};
