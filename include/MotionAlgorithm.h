#pragma once

class MotionAlgorithm {
public:
    MotionAlgorithm();
  
    explicit MotionAlgorithm(bool enabled);
  
    virtual ~MotionAlgorithm() = default;
    
    virtual float evaluate(float time) const = 0;

    bool getEnabled() const;

    void setEnabled(bool e);

private:

    bool enabled;

};
