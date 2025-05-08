#include "MotionAlgorithm.h"

class SinusoidalMotion : public MotionAlgorithm {
public:
  
    SinusoidalMotion(float amplitude, float frequency, float phase);

    float evaluate(float time) const override;

private:
    float amplitude;
    float frequency;
    float phase;
  
};
