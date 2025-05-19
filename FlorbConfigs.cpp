#include <GL/glx.h>
#include <fstream>
#include <iostream>
#include <cmath>

#include "Camera.h"
#include "FlorbConfigs.h"
#include "FlorbUtils.h"
#include "LinearMotion.h"
#include "Spotlight.h"

extern Display *display;
extern Window window;

using std::cerr;
using std::cos;
using std::endl;
using std::exception;
using std::ifstream;
using std::lock_guard;
using std::make_shared;
using std::mutex;
using std::pair;
using std::shared_ptr;
using std::sin;
using std::string;
using std::vector;

using json = nlohmann::json_abi_v3_12_0::json;

#define LOCK_CONFIGS lock_guard<mutex> lock(stateMutex)


// Implementation of class FlorbConfigs

// Static member initialization

const string FlorbConfigs::k_DefaultImagePath("images");


const string FlorbConfigs::k_DefaultTitle("Florb v0.3");


const float FlorbConfigs::k_MaxVideoFrameRate(120.0f);

const float FlorbConfigs::k_DefaultVideoFrameRate(60.0f);


const float FlorbConfigs::k_DefaultImageSwitch(5.0f);


const float FlorbConfigs::k_DefaultTransitionTime(1.0f);


const unsigned int FlorbConfigs::k_MaxSpotlights(4UL);


const float FlorbConfigs::k_DefaultRadius(0.8f);


const int FlorbConfigs::k_MaxMotes(256);


// Constructor

FlorbConfigs::FlorbConfigs() :
    imagePath(k_DefaultImagePath),

    videoFrameRate(k_DefaultVideoFrameRate),
    imageSwitch(k_DefaultImageSwitch),

    transitionMode(TransitionMode::FLIP),
    transitionOrder(TransitionOrder::ALPHABETICAL),
    transitionTime(k_DefaultTransitionTime),

    cameras(),
    
    offsetX(0.0f),
    offsetY(0.0f),
    radius(k_DefaultRadius),
    smoothness(7UL),

    shininess(1.0f),
    spotlights(),

    bounceEnabled(false),
    bounceAmplitude(0.0f),
    bounceFrequency(0.0f),

    breatheEnabled(false),
    breatheAmplitude(2, k_DefaultRadius),
    breatheFrequency(0.0f),
    
    rimStrength(0.0f),
    rimExponent(0.0f),
    rimColor(3, 0.0f),
    rimFrequency(0.0f),
    rimAnimateEnabled(false),
    rimAnimateFrequency(0.0f),

    vignetteRadius(0.0f),
    vignetteExponent(0.0f),

    moteCount(0ULL),
    motesRadius(0.0f),
    motesMaxStep(0.0f),
    motesWinkingEnabled(false),
    motesMaxOff(0.0f),
    motesColor(3, 0.0f),

    renderMode(RenderMode::FILL),
    specularMode(SpecularMode::NORMAL),

    stateMutex() { }


// Public member methods

void FlorbConfigs::load() {

    // TODO - Make into a default constant, with an overload which
    //        accepts a filename (to be passed on the command line)
    ifstream file("florb.json");
    if (!file.is_open()) {
        cerr << "[WARN] Could not open florb.json for configuration" << endl;
        return;
    }

    json config;
    try {
        file >> config;

        // Title config
        if (config.contains("title") and config["title"].is_string()) {
            setTitle(config["title"]);
        } else {
            setTitle(k_DefaultTitle);
        }

        
        // Image path config
        if (config.contains("image_path") and config["image_path"].is_string()) {
            setImagePath(config["image_path"]);
        } else {
            setImagePath(k_DefaultImagePath);
        }


        // Video configs
        if (config.contains("video") and config["video"].is_object()) {
            const auto &video(config["video"]);
            
            if (video.contains("frame_rate") and video["frame_rate"].is_number()) {
                setVideoFrameRate(video["frame_rate"]);
            }
            
            if (video.contains("image_switch") and video["image_switch"].is_number()) {
                setImageSwitch(video["image_switch"]);
            }
        }


        // Transition configs
        if (config.contains("transitions") and config["transitions"].is_object()) {
            const auto &transitions(config["transitions"]);
            
            if (transitions.contains("mode") and transitions["mode"].is_string()) {
                const auto &mode(transitions["mode"]);

                if(mode == "flip") {
                    setTransitionMode(TransitionMode::FLIP);
                } else if(mode == "blend") {
                    setTransitionMode(TransitionMode::BLEND);
                } else {
                    cerr << "Invalid transition mode value \""
                         << mode
                         << "\""
                         << endl;
                }
            }

            if (transitions.contains("order") and transitions["order"].is_string()) {
                const auto &order(transitions["order"]);

                if(order == "alphabetical") {
                    setTransitionOrder(TransitionOrder::ALPHABETICAL);
                } else if(order == "random") {
                    setTransitionOrder(TransitionOrder::RANDOM);
                } else {
                    cerr << "Invalid transition order value \""
                         << order
                         << "\""
                         << endl;
                }
            }            
            
            if (transitions.contains("time") and transitions["time"].is_number()) {
                setTransitionTime(transitions["time"]);
            }
        }
        

        // Cameras
        if (config.contains("cameras") and config["cameras"].is_array()) {
            const auto &cameras(config["cameras"]);
        
            unsigned int cameraNum(0UL);
            for (const auto &camera : cameras) {
                string name;
                vector<float> view(3, 0.0f);
                float zoom(0.0f);
              
                if (camera.contains("name") and camera["name"].is_string()) {
                    name = camera["name"];
                } else {
                    cerr << "Camera ["
                         << cameraNum
                         << "] is missing string property \"name\""
                         << endl;
                }
                
                if (camera.contains("view") and camera["view"].is_array()) {
                    const auto &viewRef(camera["view"]);

                    if (viewRef.size() == 2) {
                        float azimuth(viewRef[0]);
                        float elevation(viewRef[1]);
                        
                        view = {
                            (cos(elevation) * cos(azimuth)),
                            sin(elevation),
                            (cos(elevation) * sin(azimuth))
                        };
                    } else {
                        cerr << "Camera view vector has incorrect size ("
                             << viewRef.size()
                             << "), expected (2)"
                             << endl;
                    }
                } else {
                    cerr << "Camera ["
                         << cameraNum
                         << "] is missing array property \"view\""
                         << endl;
                }

                if (camera.contains("zoom") and camera["zoom"].is_number()) {
                    auto zoomFactor(1.0f / static_cast<float>(camera["zoom"]));
                    zoom = zoomFactor;
                }

                // Create the new camera and push it onto our collection
                this->cameras.push_back(make_shared<Camera>(name, view, zoom));
                    
                cameraNum++;
            }
        } // camera configs
        

        // Geometry configs
        if (config.contains("geometry") and config["geometry"].is_object()) {
            const auto &geometry(config["geometry"]);

            if (geometry.contains("center") and geometry["center"].is_array() and geometry["center"].size() == 2) {
                setCenter(geometry["center"][0], geometry["center"][1]);
            }

            if (geometry.contains("radius") and geometry["radius"].is_number()) {
                setRadius(geometry["radius"]);
            }
            
            if (geometry.contains("smoothness") and geometry["smoothness"].is_number()) {
                setSmoothness(geometry["smoothness"]);
            }
        }        

 
        // Light configs
        if (config.contains("light") and config["light"].is_object()) {
            const auto &light(config["light"]);

            if (light.contains("shininess") and light["shininess"].is_number()) {
                setShininess(light["shininess"]);
            }

            parseSpotlights(light);

            // Rim lighting
            if (light.contains("rim") and light["rim"].is_object()) {
                const auto &rim(light["rim"]);

                if (rim.contains("strength") and rim["strength"].is_number()) {
                    setRimStrength(rim["strength"]);
                }
                
                if (rim.contains("exponent") and rim["exponent"].is_number()) {
                    setRimExponent(rim["exponent"]);
                }
            
                if (rim.contains("color") and rim["color"].is_array()) {
                    const auto &color(rim["color"]);
                    setRimColor(color[0], color[1], color[2]);
                }
                
                if (rim.contains("frequency") and rim["frequency"].is_number()) {
                    setRimFrequency(rim["frequency"]);
                }
                
                if (rim.contains("animate") and rim["animate"].is_object()) {
                    const auto &animate(rim["animate"]);

                    if (animate.contains("enabled") and animate["enabled"].is_boolean()) {
                        setRimAnimateEnabled(animate["enabled"]);
                    }
                    
                    if (animate.contains("frequency") and animate["frequency"].is_number()) {
                        setRimAnimateFrequency(animate["frequency"]);
                    }
                } // animate configs
            } // rim configs
        } // light configs
                        
        
        // Effects configs
        if (config.contains("effects") and config["effects"].is_object()) {
            const auto &effects(config["effects"]);

            if (effects.contains("bounce") and effects["bounce"].is_object()) {
                const auto &bounce(effects["bounce"]);

                if (bounce.contains("enabled") and bounce["enabled"].is_boolean()) {
                    setBounceEnabled(bounce["enabled"]);
                }

                if (bounce.contains("amplitude") and bounce["amplitude"].is_number()) {
                    const auto &amplitude(bounce["amplitude"]);
                    setBounceAmplitude(amplitude);
                }
                
                if (bounce.contains("frequency") and bounce["frequency"].is_number()) {
                    setBounceFrequency(bounce["frequency"]);
                }
            }

            if (effects.contains("breathe") and effects["breathe"].is_object()) {
                const auto &breathe(effects["breathe"]);

                if (breathe.contains("enabled") and breathe["enabled"].is_boolean()) {
                    setBreatheEnabled(breathe["enabled"]);
                }

                if (breathe.contains("amplitude") and breathe["amplitude"].is_array()) {
                    const auto &amplitude(breathe["amplitude"]);
                    setBreatheAmplitude(amplitude[0], amplitude[1]);
                }
                
                if (breathe.contains("frequency") and breathe["frequency"].is_number()) {
                    setBreatheFrequency(breathe["frequency"]);
                }
            }
        }

        
        // Vignette configs
        if (config.contains("vignette") and config["vignette"].is_object()) {
            const auto &vignette(config["vignette"]);
            
            if (vignette.contains("radius") and vignette["radius"].is_number()) {
                setVignetteRadius(vignette["radius"]);
            }
            
            if (vignette.contains("exponent") and vignette["exponent"].is_number()) {
                setVignetteExponent(vignette["exponent"]);
            }
        }


        // Mote configs
        if (config.contains("motes") and config["motes"].is_object()) {
            const auto &motes(config["motes"]);

            if (motes.contains("count") and motes["count"].is_number_integer()) {
                auto moteCount(motes["count"]);

                if (moteCount < 0) {
                    moteCount = 0UL;
                } if (moteCount > k_MaxMotes) {
                    moteCount = k_MaxMotes;
                }
                
                setMoteCount(moteCount);
            }
            
            if (motes.contains("radius") and motes["radius"].is_number()) {
                setMotesRadius(motes["radius"]);
            }

            if (motes.contains("color") and motes["color"].is_array()) {
                if (motes.contains("color") and motes["color"].is_array()) {
                    const auto &color(motes["color"]);

                    setMotesColor(color[0], color[1], color[2]);
                }
            }            

            if (motes.contains("max_step") and motes["max_step"].is_number()) {
                setMotesMaxStep(motes["max_step"]);
            }

            if (motes.contains("winking") and motes["winking"].is_object()) {
                const auto &winking(motes["winking"]);

                if (winking.contains("enabled") and winking["enabled"].is_boolean()) {
                    setMotesWinkingEnabled(winking["enabled"]);
                }

                if (winking.contains("max_off") and winking["max_off"].is_number()) {
                    setMotesWinkingMaxOff(winking["max_off"]);
                }                
            }

            if (motes.contains("color") and motes["color"].is_array()) {
                const auto &color(motes["color"]);
                setMotesColor(color[0], color[1], color[2]);
            }
        }


        // TODO - Add tilt (center-axis rotation) config

        
        // Debug configs
        if (config.contains("debug") and config["debug"].is_object()) {
            const auto &debug(config["debug"]);

            // Rendering mode - normal fill, or wiremesh lines
            if (debug.contains("render_mode") and debug["render_mode"].is_string()) {
                if(debug["render_mode"] == "fill") {
                    setRenderMode(RenderMode::FILL);
                } else if(debug["render_mode"] == "line") {
                    setRenderMode(RenderMode::LINE);
                } else {
                    cerr << "Invalid render_mode config value \""
                         << debug["render_mode"]
                         << "\""
                         << endl;
                }
            }

            // Specular mode - normal reflections, or debug spot
            if (debug.contains("specular_mode") and debug["specular_mode"].is_string()) {
                if(debug["specular_mode"] == "normal") {
                    setSpecularMode(SpecularMode::NORMAL);
                } else if(debug["specular_mode"] == "debug") {
                    setSpecularMode(SpecularMode::DEBUG);
                } else {
                    cerr << "Invalid specular_mode config value \""
                         << debug["specular_mode"]
                         << "\""
                         << endl;
                }
            }
        }

    } catch (const exception& exc) {
        cerr << "[ERROR] Failed to parse \"florb.json\" : " << exc.what() << endl;
    }
}


// Title mutator

const string& FlorbConfigs::getTitle() const {
    LOCK_CONFIGS;
    return title;
}
  
void FlorbConfigs::setTitle(const string &t) {
    LOCK_CONFIGS;
    title = t;
    FlorbUtils::setWindowTitle(display, window, title);
}


// Image path accessor / mutator

const string& FlorbConfigs::getImagePath() const {
    LOCK_CONFIGS;
    return imagePath;
}
  
void FlorbConfigs::setImagePath(const string &p) {
    LOCK_CONFIGS;
    imagePath = p;
}


// Video accessors / mutators

float FlorbConfigs::getVideoFrameRate() const {
    LOCK_CONFIGS;
    return videoFrameRate;
}

void FlorbConfigs::setVideoFrameRate(float r) {
    LOCK_CONFIGS;

    if (videoFrameRate >= k_MaxVideoFrameRate) {
        videoFrameRate = k_MaxVideoFrameRate;
    } else {
        videoFrameRate = r;
    }
}

float FlorbConfigs::getImageSwitch() const {
    LOCK_CONFIGS;
    return imageSwitch;
}

void FlorbConfigs::setImageSwitch(float s) {
    LOCK_CONFIGS;
    imageSwitch = s;
}


// Transition mode accessor / mutator

FlorbConfigs::TransitionMode FlorbConfigs::getTransitionMode() const {
    LOCK_CONFIGS;
    return transitionMode;
}

void FlorbConfigs::setTransitionMode(TransitionMode t) {
    LOCK_CONFIGS;
    transitionMode = t;
}

FlorbConfigs::TransitionOrder FlorbConfigs::getTransitionOrder() const {
    LOCK_CONFIGS;
    return transitionOrder;
}

void FlorbConfigs::setTransitionOrder(TransitionOrder o) {
    LOCK_CONFIGS;
    transitionOrder = o;
}

float FlorbConfigs::getTransitionTime() const {
    LOCK_CONFIGS;
    return transitionTime;
}

void FlorbConfigs::setTransitionTime(float t) {
    LOCK_CONFIGS;
    transitionTime = t;
}


// Cameras accessor

const vector<shared_ptr<Camera>>& FlorbConfigs::getCameras() const {
    LOCK_CONFIGS;
    return cameras;
}


// Geometry accessors / mutators

pair<float, float> FlorbConfigs::getCenter() const {
    LOCK_CONFIGS;
    return {offsetX, offsetY};
}

void FlorbConfigs::setCenter(float x, float y) {
    LOCK_CONFIGS;
    offsetX = x;
    offsetY = y;
}

float FlorbConfigs::getRadius() const {
    LOCK_CONFIGS;
    return radius;
}

void FlorbConfigs::setRadius(float r) {
    LOCK_CONFIGS;
    radius = r;
}

unsigned int FlorbConfigs::getSmoothness() const {
    LOCK_CONFIGS;
    return smoothness;
}

void FlorbConfigs::setSmoothness(unsigned int s) {
    LOCK_CONFIGS;
    smoothness = s;
}


// Spotlights accessor

const vector<shared_ptr<Spotlight>>& FlorbConfigs::getSpotlights() const {
    LOCK_CONFIGS;
    return spotlights;
}


// Bounce accessors / mutators

bool FlorbConfigs::getBounceEnabled() const {
    LOCK_CONFIGS;
    return bounceEnabled;
}

void FlorbConfigs::setBounceEnabled(bool e) {
    LOCK_CONFIGS;
    bounceEnabled = e;
}

float FlorbConfigs::getBounceAmplitude() const {
    LOCK_CONFIGS;
    return bounceAmplitude;
}

void FlorbConfigs::setBounceAmplitude(float a) {
    LOCK_CONFIGS;
    bounceAmplitude = a;
}

float FlorbConfigs::getBounceFrequency() const {
    LOCK_CONFIGS;
    return bounceFrequency;
}

void FlorbConfigs::setBounceFrequency(float f) {
    LOCK_CONFIGS;
    bounceFrequency = f;
}


// Breathe accessors / mutators

bool FlorbConfigs::getBreatheEnabled() const {
    LOCK_CONFIGS;
    return breatheEnabled;
}

void FlorbConfigs::setBreatheEnabled(bool e) {
    LOCK_CONFIGS;
    breatheEnabled = e;
}

const vector<float>& FlorbConfigs::getBreatheAmplitude() const {
    LOCK_CONFIGS;
    return breatheAmplitude;
}

void FlorbConfigs::setBreatheAmplitude(float min, float max) {
    LOCK_CONFIGS;
    breatheAmplitude[0] = min;
    breatheAmplitude[1] = max;
}

float FlorbConfigs::getBreatheFrequency() const {
    LOCK_CONFIGS;
    return breatheFrequency;
}

void FlorbConfigs::setBreatheFrequency(float f) {
    LOCK_CONFIGS;
    breatheFrequency = f;
}


// Shininess accessor / mutator

float FlorbConfigs::getShininess() const {
    LOCK_CONFIGS;
    return shininess;
}

void FlorbConfigs::setShininess(float s) {
    LOCK_CONFIGS;
    shininess = s;
}


// Rim light accessors / mutators

float FlorbConfigs::getRimStrength() const {
    LOCK_CONFIGS;
    return rimStrength;
}

void FlorbConfigs::setRimStrength(float s) {
    LOCK_CONFIGS;
    rimStrength = s;
}

float FlorbConfigs::getRimExponent() const {
    LOCK_CONFIGS;
    return rimExponent;
}

void FlorbConfigs::setRimExponent(float s) {
    LOCK_CONFIGS;
    rimExponent = s;
}

const vector<float>& FlorbConfigs::getRimColor() const {
    LOCK_CONFIGS;
    return rimColor;
}

void FlorbConfigs::setRimColor(float r, float g, float b) {
    LOCK_CONFIGS;
    rimColor[0] = r;
    rimColor[1] = g;
    rimColor[2] = b;
}

float FlorbConfigs::getRimFrequency() const {
    LOCK_CONFIGS;
    return rimFrequency;
}

void FlorbConfigs::setRimFrequency(float f) {
    LOCK_CONFIGS;
    rimFrequency = f;
}

bool FlorbConfigs::getRimAnimateEnabled() const {
    LOCK_CONFIGS;
    return rimAnimateEnabled;
}

void FlorbConfigs::setRimAnimateEnabled(bool a) {
    LOCK_CONFIGS;
    rimAnimateEnabled = a;
}

float FlorbConfigs::getRimAnimateFrequency() const {
    LOCK_CONFIGS;
    return rimAnimateFrequency;
}

void FlorbConfigs::setRimAnimateFrequency(float f) {
    LOCK_CONFIGS;
    rimAnimateFrequency = f;
}


// Vignette accessors / mutators

float FlorbConfigs::getVignetteRadius() const {
    LOCK_CONFIGS;
    return vignetteRadius;
}

void FlorbConfigs::setVignetteRadius(float r) {
    LOCK_CONFIGS;
    vignetteRadius = r;
}

float FlorbConfigs::getVignetteExponent() const {
    LOCK_CONFIGS;
    return vignetteExponent;
}

void FlorbConfigs::setVignetteExponent(float r) {
    LOCK_CONFIGS;
    vignetteExponent = r;
}


// Mote accessors / mutators

unsigned int FlorbConfigs::getMoteCount() const {
    LOCK_CONFIGS;
    return moteCount;
}

void FlorbConfigs::setMoteCount(unsigned int c) {
    LOCK_CONFIGS;
    moteCount = c;
}

float FlorbConfigs::getMotesRadius() const {
    LOCK_CONFIGS;
    return motesRadius;
}

void FlorbConfigs::setMotesRadius(float r) {
    LOCK_CONFIGS;
    motesRadius = r;
}    

float FlorbConfigs::getMotesMaxStep() const {
    LOCK_CONFIGS;
    return motesMaxStep;
}

void FlorbConfigs::setMotesMaxStep(float s) {
    LOCK_CONFIGS;
    motesMaxStep = s;
}


bool FlorbConfigs::getMotesWinkingEnabled() const {
    LOCK_CONFIGS;
    return motesWinkingEnabled;
}

void FlorbConfigs::setMotesWinkingEnabled(bool e) {
    LOCK_CONFIGS;
    motesWinkingEnabled = e;
}

float FlorbConfigs::getMotesWinkingMaxOff() const {
    LOCK_CONFIGS;
    return motesMaxOff;
}

void FlorbConfigs::setMotesWinkingMaxOff(float m) {
    LOCK_CONFIGS;
    motesMaxOff = m;
}

const vector<float>& FlorbConfigs::getMotesColor() const {
    LOCK_CONFIGS;
    return motesColor;
}

void FlorbConfigs::setMotesColor(float r, float g, float b) {
    LOCK_CONFIGS;
    motesColor[0] = r;
    motesColor[1] = g;
    motesColor[2] = b;
}


// Debug accessors / mutators

FlorbConfigs::RenderMode FlorbConfigs::getRenderMode() const {
    LOCK_CONFIGS;
    return renderMode;
}

void FlorbConfigs::setRenderMode(FlorbConfigs::RenderMode r) {
    LOCK_CONFIGS;
    renderMode = r;
}

FlorbConfigs::SpecularMode FlorbConfigs::getSpecularMode() const {
    LOCK_CONFIGS;
    return specularMode;
}

void FlorbConfigs::setSpecularMode(FlorbConfigs::SpecularMode s) {
    LOCK_CONFIGS;
    specularMode = s;
}


// Private methods

void FlorbConfigs::parseSpotlights(const json &light) {            
    if (light.contains("spotlights") and light["spotlights"].is_array()) {
        const auto &spotlights(light["spotlights"]);
    
        unsigned int spotlightNum(0UL);
        for (const auto &spotlight : spotlights) {
            string name;
            vector<float> direction(3, 0.0f);
            float intensity(0.0f);
            vector<float> color(3, 0.0f);

            // Limit the number of spotlights to a maximum
            if (spotlightNum >= k_MaxSpotlights) {
                cerr << "Number of spotlights exceeds maximum of ("
                     << k_MaxSpotlights
                     << ")"
                     << endl;
                break;
            }

            if (spotlight.contains("name") and spotlight["name"].is_string()) {
                name = spotlight["name"];
            } else {
                cerr << "Spotlight ["
                     << spotlightNum
                     << "] is missing string property \"name\""
                     << endl;
            }
            
            if (spotlight.contains("direction") and spotlight["direction"].is_array()) {
                const auto &directionRef(spotlight["direction"]);

                if (directionRef.size() == 2) {
                    float azimuth(directionRef[0]);
                    float elevation(directionRef[1]);

                    direction = {
                        (cos(elevation) * cos(azimuth)),
                        sin(elevation),
                        (cos(elevation) * sin(azimuth))
                    };
                } else {
                    cerr << "Spotlight \""
                         << name
                         << "\" direction vector has incorrect size ("
                         << directionRef.size()
                         << "), expected (2)"
                         << endl;
                }
            } else {
                cerr << "Spotlight ["
                     << spotlightNum
                     << "] is missing array property \"direction\""
                     << endl;
            }
    
            if (spotlight.contains("intensity") and spotlight["intensity"].is_number()) {
                intensity = spotlight["intensity"];
            }

            if (spotlight.contains("color") and spotlight["color"].is_array()) {
                const auto &colorRef(spotlight["color"]);
                color = {colorRef[0], colorRef[1], colorRef[2]};
            }

            // Parse any optional movement object
            shared_ptr<LinearMotion> motionPtr;
            if (spotlight.contains("motion") and spotlight["motion"].is_object()) {
                const auto &motion(spotlight["motion"]);

                if (motion.contains("type") and motion["type"].is_string()) {
                    const auto &type(motion["type"]);
                    bool enabled(false);

                    if (motion.contains("enabled") and motion["enabled"].is_boolean()) {
                        enabled = motion["enabled"];
                    }
                    
                    // TODO - Convert to a PluggableFactories pattern perhaps
                    if (type == "linear") {
                        vector<float> offsets(3, 0.0f);
                        vector<float> speeds(3, 0.0f);

                        if (motion.contains("offsets") and motion["offsets"].is_array()) {
                            const auto &offsetsRef(motion["offsets"]);
                            offsets = {offsetsRef[0], offsetsRef[1], offsetsRef[2]};
                        }
                        
                        if (motion.contains("speeds") and motion["speeds"].is_array()) {
                            const auto &speedsRef(motion["speeds"]);
                            speeds = {speedsRef[0], speedsRef[1], speedsRef[2]};
                        }

                        motionPtr = make_shared<LinearMotion>(enabled, offsets, speeds);
                    } else {
                        cerr << "Unrecognized \"motion\" type \"" << type << "\"" << endl;
                    }
                }
            } // motion parsing

            // Create the new spotlight and push it onto our collection
            this->spotlights.push_back(make_shared<Spotlight>(name,
                                                              direction,
                                                              intensity,
                                                              color,
                                                              motionPtr));
                
            spotlightNum++;
        }
    } // spotlight configs
}
