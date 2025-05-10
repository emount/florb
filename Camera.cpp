#include "Camera.h"

// Namespace using directives

using std::lock_guard;
using std::mutex;
using std::string;
using std::vector;


// Implementation of class Camera

// Constructor

Camera::Camera(const string &name,
               const vector<float> &view,
               float zoom) :
    name(name),
    view(view),
    zoom(zoom) { }


// Accessors / mutators

const vector<float>& Camera::getView() const {
    lock_guard<mutex> lock(stateMutex);
    return view;
}

void Camera::setView(float alpha, float beta, float phi) {
    lock_guard<mutex> lock(stateMutex);
    view[0] = alpha;
    view[1] = beta;
    view[2] = phi;
}

float Camera::getZoom() const {
    lock_guard<mutex> lock(stateMutex);
    return zoom;
}

void Camera::setZoom(float z) {
    lock_guard<mutex> lock(stateMutex);
    zoom = z;
}
