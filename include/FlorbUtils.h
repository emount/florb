#pragma once

#include <X11/Xlib.h>
#include <iostream>
#include <chrono>
#include <string>

namespace FlorbUtils {

    GLuint createDebugTexture();

    void dumpTexture(GLuint textureID,
		     int width,
		     int height,
		     GLenum format,
		     int channels);

    void setWindowTitle(Display *display,
			Window window,
			const std::string &title);
  
    void glCheck(const std::string &str);

}
