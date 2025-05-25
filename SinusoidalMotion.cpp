#include <iostream>
#include <math.h>

#include "SinusoidalMotion.h"

using std::cerr;
using std::endl;

// Implementation of class SinusoidalMotion

// Constructor

SinusoidalMotion::SinusoidalMotion() { }

SinusoidalMotion::SinusoidalMotion(bool enabled,
                                   float bias,
                                   float amplitude,
                                   float frequency,
                                   float phase) :
    MotionAlgorithm(enabled),
    bias(bias),
    amplitude(amplitude),
    frequency(frequency),
    phase(phase) { }


float SinusoidalMotion::evaluate(float time) const {
    float evaluated;

    if (getEnabled()) {
        float sinusoid(sinf(2.0f * M_PI * frequency * time + phase));
        
        evaluated = (bias + (amplitude * sinusoid));
    } else {
        evaluated = (bias + amplitude);
    }
    
    return evaluated;
}
