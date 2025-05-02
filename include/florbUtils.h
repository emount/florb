#pragma once

#include <string>

namespace FlorbUtils {

    GLuint createDebugTexture();

    void dumpTexture(GLuint textureID,
		     int width,
		     int height,
		     GLenum format,
		     int channels);
    
    void glCheck(const std::string &str);

}
