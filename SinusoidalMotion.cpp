#include <iostream>
#include <math.h>

#include "SinusoidalMotion.h"

using std::cerr;
using std::endl;

// Implementation of class SinusoidalMotion

// Constructor

SinusoidalMotion::SinusoidalMotion() { }

SinusoidalMotion::SinusoidalMotion(float bias,
				   float amplitude,
				   float frequency,
				   float phase) :
    MotionAlgorithm(),
    bias(bias),
    amplitude(amplitude),
    frequency(frequency),
    phase(phase) { }


float SinusoidalMotion::evaluate(float time) const {
    float sinusoid(sinf(2.0f * M_PI * frequency * time + phase));
    
    return (bias + (amplitude * sinusoid));
}
