#include "Camera.h"


// Namespace using directives

using std::shared_ptr;
using std::string;


// Implementation of class Camera

// Constructor

Camera::Camera(const string &name,
               shared_ptr<FlorbConfigs> configs) :
    name(name),
    configs(configs) { }

