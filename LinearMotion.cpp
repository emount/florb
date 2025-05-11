#include <iostream>
#include <math.h>

#include "LinearMotion.h"

using std::cerr;
using std::endl;

// Implementation of class LinearMotion

// Constructor

LinearMotion::LinearMotion() { }

LinearMotion::LinearMotion(bool enabled, float offset, float speed) :
    MotionAlgorithm(enabled),
    offset(offset),
    speed(speed) { }


float LinearMotion::evaluate(float time) const {
    return offset + (speed * time);
}
