class MotionAlgorithm {
public:
    MotionAlgorithm();
  
    explicit MotionAlgorithm(bool enabled);
  
    virtual ~MotionAlgorithm() = default;
    
    virtual float evaluate(float time) const = 0;

    bool get_enabled() const;

private:
    bool enabled;
};
