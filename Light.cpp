#include "Light.h"

// Namespace using directives

using std::lock_guard;
using std::mutex;
using std::string;
using std::vector;


// Implementation of class Light

// Constructor

Light::Light(const string &name,
             const vector<float> &direction,
             float intensity,
             const vector<float> &color) :
    name(name),
    direction(direction),
    intensity(intensity),
    color(color) { }


// Accessors / mutators

const vector<float>& Light::getDirection() const {
    lock_guard<mutex> lock(stateMutex);
    return direction;
}

void Light::setDirection(float alpha, float beta, float phi) {
    lock_guard<mutex> lock(stateMutex);
    direction[0] = alpha;
    direction[1] = beta;
    direction[2] = phi;
}

float Light::getIntensity() const {
    lock_guard<mutex> lock(stateMutex);
    return intensity;
}

void Light::setIntensity(float i) {
    lock_guard<mutex> lock(stateMutex);
    intensity = i;
}

const vector<float>& Light::getColor() const {
    lock_guard<mutex> lock(stateMutex);
    return color;
}

void Light::setColor(float r, float g, float b) {
    lock_guard<mutex> lock(stateMutex);
    color[0] = r;
    color[1] = g;
    color[2] = b;
}
