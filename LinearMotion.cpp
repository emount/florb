#include <exception>
#include <math.h>
#include <sstream>
#include <string>

#include "LinearMotion.h"

// Namespace using directives
using std::ostringstream;
using std::runtime_error;
using std::string;
using std::vector;


// Implementation of class LinearMotion

// Constructor

LinearMotion::LinearMotion(bool enabled, float offset, float speed) :
    LinearMotion(enabled, vector<float>(1, offset), vector(1, speed)) { }

LinearMotion::LinearMotion(bool enabled,
                           const vector<float> &offsets,
                           const vector<float> &speeds) :
    MultiMotion(enabled),
    offsets(offsets),
    speeds(speeds) {
    static const string k_Context(__PRETTY_FUNCTION__);

    // Sanity-check collection sizes
    if (offsets.size() != speeds.size()) {
        ostringstream excStream;
        excStream << k_Context
                  << " : Size of offsets ("
                  << offsets.size()
                  << ") and speeds ("
                  << speeds.size()
                  << ") do not match";
      
        throw runtime_error(excStream.str());
    }
}

vector<float> LinearMotion::vectorEvaluate(float time) const {
    vector<float> results;
    
    for (size_t i = 0; i < offsets.size(); i++) {
        results.push_back(offsets[i] + (speeds[i] * time));
    }
    
    return results;
}
