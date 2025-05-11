#include "MotionAlgorithm.h"

class LinearMotion : public MotionAlgorithm {
public:

    LinearMotion();
  
    LinearMotion(bool enabled, float offset, float speed);

    float evaluate(float time) const override;

private:
    float offset;
    float speed;
  
};
