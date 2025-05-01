#include <GL/glew.h>
#include <iostream>
#include <vector>
#include "florbUtils.h"

using std::cerr;
using std::endl;
using std::min;
using std::string;
using std::vector;

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

void FlorbUtils::glCheck(const string &str) {
    GLenum err;
  
    while ((err = glGetError()) != GL_NO_ERROR) {
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
	cerr << ")]" << endl;
    }
}
