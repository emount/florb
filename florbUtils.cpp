#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <GL/glew.h>
#include <GL/glu.h>
#include <iomanip>
#include <string>
#include <vector> 

#include "florbUtils.h"

namespace chrono = std::chrono;

using chrono::system_clock;
using chrono::time_point;

using std::cerr;
using std::endl;
using std::localtime;
using std::min;
using std::ostream;
using std::put_time;
using std::string;
using std::time_t;
using std::tm;
using std::vector;

GLuint FlorbUtils::createDebugTexture() {
    GLuint textureID;
    unsigned char redPixel[] = { 255, 0, 0, 255 };

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, redPixel);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return textureID;
}

void FlorbUtils::dumpTexture(GLuint textureID,
			     int width,
			     int height,
			     GLenum format,
			     int channels) {
    int bytesPerPixel = (channels == 4) ? 4 : 3;
    vector<unsigned char> pixels(width * height * bytesPerPixel);

    glBindTexture(GL_TEXTURE_2D, textureID);
    glGetTexImage(GL_TEXTURE_2D, 0, format, GL_UNSIGNED_BYTE, pixels.data());

    cerr << "[TEXTURE DUMP] Texture ID: " << textureID
              << " (" << width << "x" << height << ", " << channels << " channels)\n";

    for (int i = 0; i < min(16, (int)pixels.size()); ++i) {
        cerr << (int)pixels[i] << " ";
    }
    cerr << endl;
}

void FlorbUtils::setWindowTitle(Display *display, Window window, const string &title) {
    XStoreName(display, window, title.c_str());

    // Set modern window manager title
    XTextProperty windowName;
    char* name = const_cast<char*>(title.c_str());
    if (XStringListToTextProperty(&name, 1, &windowName) != 0) {
        XSetWMName(display, window, &windowName);
        XFree(windowName.value);
    }
}

void FlorbUtils::glCheck(const string &str) {
    GLenum err;
  
    while ((err = glGetError()) != GL_NO_ERROR) {
      const GLubyte* errStr = gluErrorString(err);
      cerr << "[" << str << " : GL ERROR (";
        switch(err) {
        case GL_INVALID_ENUM:
           cerr << "Invalid Enum";
	   break;
        
        case GL_INVALID_OPERATION:
           cerr << "Invalid Operation";
	   break;
        
        case GL_INVALID_VALUE:
           cerr << "Invalid Value";
	   break;
        
        default:
	    cerr << err;
	}
	cerr << ")] : \""
	     << errStr
	     << "\""
	     << endl;
    }
}
