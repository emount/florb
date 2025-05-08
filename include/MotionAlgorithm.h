class MotionAlgorithm {
public:
    virtual ~MotionAlgorithm() = default;
    
    virtual float evaluate(float time) const = 0;
};
