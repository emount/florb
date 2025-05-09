#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <memory>

class FlorbConfigs : public std::enable_shared_from_this<FlorbConfigs> {

public:
  
    // Enumerated type for render mode
    enum class RenderMode { FILL, LINE };

    // Enumerated type for specular mode
    enum class SpecularMode { NORMAL, DEBUG };

public:
    FlorbConfigs();

    void load();

    const std::string& getTitle() const;
    void setTitle(const std::string& t);

  
    const std::string& getImagePath() const;
    void setImagePath(const std::string& p);

  
    const std::vector<std::shared_ptr<Camera>>& getCameras() const;

  
    float getVideoFrameRate() const;
    void setVideoFrameRate(float r);

    float getImageSwitch() const;
    void setImageSwitch(float s);

  
    std::pair<float, float> getCenter() const;
    void setCenter(float x, float y);

    float getRadius() const;
    void setRadius(float r);

    unsigned int getSmoothness() const;
    void setSmoothness(unsigned int s);

  
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

  
    const std::vector<float>& getLightDirection() const;
    void setLightDirection(float alpha, float beta, float phi);

    float getLightIntensity() const;
    void setLightIntensity(float i);

    float getShininess() const;
    void setShininess(float s);

    const std::vector<float>& getLightColor() const;
    void setLightColor(float r, float g, float b);

  
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
    
    float getMoteRadius() const;
    void setMoteRadius(float r);
    
    float getMoteMaxStep() const;
    void setMoteMaxStep(float s);
    
    const std::vector<float>& getMoteColor() const;
    void setMoteColor(float r, float g, float b);

  
    RenderMode getRenderMode() const;
    void setRenderMode(RenderMode r);

    SpecularMode getSpecularMode() const;
    void setSpecularMode(SpecularMode s);  

public:
  
    static const std::string k_DefaultImagePath;
  
    static const float k_DefaultRadius;

private:
  
    std::string imagePath;
  
    std::string title;
  
    float videoFrameRate;
    float imageSwitch;
  
    std::vector<std::shared_ptr<Camera>> cameras;

    float offsetX;
    float offsetY;
    float radius;
    unsigned int smoothness;

    bool bounceEnabled;
    float bounceAmplitude;
    float bounceFrequency;

    bool breatheEnabled;
    std::vector<float> breatheAmplitude;
    float breatheFrequency;

    std::vector<float> lightDirection;
    float lightIntensity;
    float shininess;
    std::vector<float> lightColor;
    
    float rimStrength;
    float rimExponent;
    std::vector<float> rimColor;
    float rimFrequency;
    bool rimAnimateEnabled;
    float rimAnimateFrequency;

    float vignetteRadius;
    float vignetteExponent;

    unsigned int moteCount;
    float moteRadius;
    float moteMaxStep;
    std::vector<float> moteColor;

    RenderMode renderMode;
    SpecularMode specularMode;
      
    mutable std::mutex stateMutex;

    static const std::string k_DefaultTitle;

    static const float k_MaxVideoFrameRate;
    static const float k_DefaultVideoFrameRate;
    static const float k_DefaultImageSwitch;
  
    static const int k_MaxMotes;
};
