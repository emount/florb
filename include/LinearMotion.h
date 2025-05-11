#include "MultiMotion.h"

class LinearMotion : public MultiMotion {
public:
  
    LinearMotion(bool enabled, float offset, float speed);

    LinearMotion(bool enabled,
                 const std::vector<float> &offsets,
                 const std::vector<float> &speeds);

    virtual std::vector<float> vectorEvaluate(float time) const override;

private:
    std::vector<float> offsets;
    std::vector<float> speeds;
  
};
