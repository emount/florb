#pragma once

#include <X11/Xlib.h>
#include <iostream>
#include <chrono>
#include <stdint.h>
#include <string>

namespace FlorbUtils {

    GLuint createTexture(std::uint32_t r,
                         std::uint32_t g,
                         std::uint32_t b,
                         std::uint32_t a);

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
