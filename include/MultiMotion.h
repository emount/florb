#pragma once

#include <vector>

#include "MotionAlgorithm.h"

class MultiMotion : public MotionAlgorithm {
public:
    explicit MultiMotion(bool enabled);
  
    virtual ~MultiMotion() = default;
    
    virtual float evaluate(float time) const;

    virtual std::vector<float> vectorEvaluate(float time) const = 0;

private:
    bool enabled;
};
