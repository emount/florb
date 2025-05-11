#include "LinearMotion.h"
#include "Spotlight.h"

// Namespace using directives

using std::lock_guard;
using std::mutex;
using std::shared_ptr;
using std::string;
using std::vector;


// Implementation of class Spotlight

// Constructor

Spotlight::Spotlight(const string &name,
                     const vector<float> &direction,
                     float intensity,
                     const vector<float> &color) :
    name(name),
    direction(direction),
    intensity(intensity),
    color(color),
    linear() { }

Spotlight::Spotlight(const string &name,
                     const vector<float> &direction,
                     float intensity,
                     const vector<float> &color,
                     shared_ptr<LinearMotion> linear) :
  Spotlight(name, direction, intensity, color) {
    linear = linear;
}


// Accessors / mutators

const vector<float>& Spotlight::getDirection() const {
    lock_guard<mutex> lock(stateMutex);
    return direction;
}

void Spotlight::setDirection(float alpha, float beta, float phi) {
    lock_guard<mutex> lock(stateMutex);
    direction[0] = alpha;
    direction[1] = beta;
    direction[2] = phi;
}

float Spotlight::getIntensity() const {
    lock_guard<mutex> lock(stateMutex);
    return intensity;
}

void Spotlight::setIntensity(float i) {
    lock_guard<mutex> lock(stateMutex);
    intensity = i;
}

const vector<float>& Spotlight::getColor() const {
    lock_guard<mutex> lock(stateMutex);
    return color;
}

void Spotlight::setColor(float r, float g, float b) {
    lock_guard<mutex> lock(stateMutex);
    color[0] = r;
    color[1] = g;
    color[2] = b;
}
