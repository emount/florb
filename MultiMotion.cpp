#include <math.h>

#include "MultiMotion.h"

// Implementation of class LinearMotion

// Constructor

MultiMotion::MultiMotion(bool enabled) :
    MotionAlgorithm(enabled) { }


float MultiMotion::evaluate(float time) const {
    // Polymorphically invoke our subclass' multi-dimensional overload
    auto vectorResult(vectorEvaluate(time));

    // Return the two-norm (Euclidean distance) of the vector quantity
    float sum(0.0f);
    for (size_t i = 0; i < vectorResult.size(); i++) {
        sum += vectorResult[i];
    }
    
    return sqrt(sum);
}
