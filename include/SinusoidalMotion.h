#include "MotionAlgorithm.h"

class SinusoidalMotion : public MotionAlgorithm {
public:

    SinusoidalMotion();
  
    SinusoidalMotion(bool enabled,
                     float bias,
                     float amplitude,
                     float frequency,
                     float phase);

    float evaluate(float time) const override;

private:

    float bias;
    float amplitude;
    float frequency;
    float phase;

};
