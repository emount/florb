#pragma once

#include <memory>
#include <string>


class FlorbConfigs;

class Camera {

public:

    // Constructor
    Camera(const std::string &name,
           std::shared_ptr<FlorbConfigs> configs);

private:

    const std::string name;

    std::shared_ptr<FlorbConfigs> configs;
    
};
