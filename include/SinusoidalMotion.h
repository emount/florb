#include "MotionAlgorithm.h"

class SinusoidalMotion : public MotionAlgorithm {
public:
  
    SinusoidalMotion(float amplitude,
		     float frequency,
		     float phase = 0.0f);

    float evaluate(float time) const override;

private:
    float amplitude;
    float frequency;
    float phase;
  
};
