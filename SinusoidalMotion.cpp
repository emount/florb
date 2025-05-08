#include <cmath>

#include "SinusoidalMotion.h"

// Implementation of class SinusoidalMotion

// Constructor

SinusoidalMotion::SinusoidalMotion(float amplitude,
				   float frequency,
				   float phase) :
    MotionAlgorithm(),
    amplitude(amplitude),
    frequency(frequency),
    phase(phase) { }


float SinusoidalMotion::evaluate(float time) const {
    return (amplitude * sinf(frequency * time + phase));
}
