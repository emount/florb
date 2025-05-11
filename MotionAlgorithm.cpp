#include "MotionAlgorithm.h"


// Implementation of class MotionAlgorithm

// Constructor

MotionAlgorithm::MotionAlgorithm() :
    enabled(false) { }

MotionAlgorithm::MotionAlgorithm(bool enabled) :
    enabled(enabled) { }

bool MotionAlgorithm::getEnabled() const {
    return enabled;
}
