#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <memory>

#include "nlohmann/json.hpp"

// Class forward references
class Camera;
class MotionAlgorithm;
class Spotlight;


// Declaration of class FlorbConfigs
class FlorbConfigs : public std::enable_shared_from_this<FlorbConfigs> {

    // Public type definitions
public:
  
    // Enumerated type for image transition mode
    enum class TransitionMode { FLIP, BLEND };

    // Enumerated type for image transition order
    enum class TransitionOrder { ALPHABETICAL, RANDOM };
  
    // Enumerated type for render mode
    enum class RenderMode { FILL, LINE };

    // Enumerated type for specular mode
    enum class SpecularMode { NORMAL, DEBUG };


    // Constructor
public:
        
    FlorbConfigs();


    // Public interface methods
public:

    void load();

    const std::string& getTitle() const;
    void setTitle(const std::string& t);

  
    const std::string& getImagePath() const;
    void setImagePath(const std::string& p);


    TransitionMode getTransitionMode() const;
    void setTransitionMode(TransitionMode t);

    TransitionOrder getTransitionOrder() const;
    void setTransitionOrder(TransitionOrder o);

    float getTransitionTime() const;
    void setTransitionTime(float t);

  
    float getVideoFrameRate() const;
    void setVideoFrameRate(float r);

    float getImageSwitch() const;
    void setImageSwitch(float s);

    const std::vector<std::shared_ptr<Camera>>& getCameras() const;

  
    std::pair<float, float> getCenter() const;
    void setCenter(float x, float y);

    float getRadius() const;
    void setRadius(float r);

    unsigned int getSmoothness() const;
    void setSmoothness(unsigned int s);

  
    float getShininess() const;
    void setShininess(float s);
  
    const std::vector<std::shared_ptr<Spotlight>>& getSpotlights() const;

  
    bool getBounceEnabled() const;
    void setBounceEnabled(bool e);

    float getBounceAmplitude() const;
    void setBounceAmplitude(float a);

    float getBounceFrequency() const;
    void setBounceFrequency(float f);

  
    bool getBreatheEnabled() const;
    void setBreatheEnabled(bool e);

    const std::vector<float>& getBreatheAmplitude() const;
    void setBreatheAmplitude(float min, float max);

    float getBreatheFrequency() const;
    void setBreatheFrequency(float f);

  
    float getRimStrength() const;
    void setRimStrength(float s);

    float getRimExponent() const;
    void setRimExponent(float e);

    const std::vector<float>& getRimColor() const;
    void setRimColor(float r, float g, float b);

    float getRimFrequency() const;
    void setRimFrequency(float f);

    bool getRimAnimateEnabled() const;
    void setRimAnimateEnabled(bool a);

    float getRimAnimateFrequency() const;
    void setRimAnimateFrequency(float f);

  
    float getVignetteRadius() const;
    void setVignetteRadius(float r);

    float getVignetteExponent() const;
    void setVignetteExponent(float e);

  
    unsigned int getMoteCount() const;
    void setMoteCount(unsigned int c);
    
    float getMotesRadius() const;
    void setMotesRadius(float r);
    
    float getMotesMaxStep() const;
    void setMotesMaxStep(float s);

    bool getMotesWinkingEnabled() const;
    void setMotesWinkingEnabled(bool e);

    float getMotesWinkingMaxOff() const;
    void setMotesWinkingMaxOff(float m);
    
    const std::vector<float>& getMotesColor() const;
    void setMotesColor(float r, float g, float b);


    bool getFlutterEnabled(void) const;
    void setFlutterEnabled(bool e);

    float getFlutterAmplitude(void) const;
    void setFlutterAmplitude(float a);

    float getFlutterFrequency(void) const;
    void setFlutterFrequency(float f);

    float getFlutterSpeed(void) const;
    void setFlutterSpeed(float s);

  
    RenderMode getRenderMode() const;
    void setRenderMode(RenderMode r);

    SpecularMode getSpecularMode() const;
    void setSpecularMode(SpecularMode s);  

    
    // Public attributes
public:
  
    static const std::string k_DefaultImagePath;
  
    static const float k_DefaultRadius;

    
    // Private helper methods
private:

    void parseSpotlights(const nlohmann::json &light);


    // Private attributes
private:
  
    std::string imagePath;
  
    std::string title;
  
    float videoFrameRate;
    float imageSwitch;

    TransitionMode transitionMode;
    TransitionOrder transitionOrder;
    float transitionTime;
  
    std::vector<std::shared_ptr<Camera>> cameras;

    float offsetX;
    float offsetY;
    float radius;
    unsigned int smoothness;

    float shininess;
    std::vector<std::shared_ptr<Spotlight>> spotlights;

    bool bounceEnabled;
    float bounceAmplitude;
    float bounceFrequency;

    bool breatheEnabled;
    std::vector<float> breatheAmplitude;
    float breatheFrequency;

    float rimStrength;
    float rimExponent;
    std::vector<float> rimColor;
    float rimFrequency;
    bool rimAnimateEnabled;
    float rimAnimateFrequency;

    float vignetteRadius;
    float vignetteExponent;

    unsigned int moteCount;
    float motesRadius;
    float motesMaxStep;
    bool motesWinkingEnabled;
    float motesMaxOff;
    std::vector<float> motesColor;

    bool flutterEnabled;
    float flutterAmplitude;
    float flutterFrequency;
    float flutterSpeed;

    RenderMode renderMode;
    SpecularMode specularMode;
      
    mutable std::mutex stateMutex;

    static const std::string k_DefaultTitle;

    static const float k_MaxVideoFrameRate;
    static const float k_DefaultVideoFrameRate;
    static const float k_DefaultImageSwitch;

    static const float k_DefaultTransitionTime;

    static const unsigned int k_MaxSpotlights;
  
    static const int k_MaxMotes;
    
};
