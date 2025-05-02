// flower.cpp
#include <GL/glew.h>
#include <iostream>

#include "flower.h"
#include "florbUtils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Uncomment to replace image data with a red texture
// #define DEBUG_IMAGES

using std::cerr;
using std::endl;
using std::string;

Flower::Flower(const std::string& filename) :
    filename(filename) {
    stbi_set_flip_vertically_on_load(true);
    int width, height, channels;    
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 0);
    
    if (!data) {
        throw std::runtime_error("Failed to load image: " + filename);
    } else {
        std::cerr << "[DEBUG] Loaded image : "
		  << filename
		  << " ("
		  << width
		  << "x"
		  << height
		  << "), "
		  << channels
	  	  << " channels, assigned texture ID ("
		  << textureID
		  << ")"
		  << endl;
    }

    GLenum format;
    if (channels == 1)
        format = GL_RED;
    else if (channels == 3)
        format = GL_RGB;
    else if (channels == 4)
        format = GL_RGBA;
    else
        throw std::runtime_error("Unsupported channel count");
    
    static_cast<void>(format);

    glGenTextures(1, &textureID);

    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLint texWidth = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texWidth);
    std::cerr << "Bound texture ID " << textureID << " has width " << texWidth << "\n";
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

#ifdef DEBUG_IMAGES
    unsigned char red[4] = { 255, 0, 0, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, red);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
#endif

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[ERROR] OpenGL texture upload failed: " << gluErrorString(err) << "\n";
    }
    
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
}

Flower::~Flower() {
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
}

const string& Flower::getFilename() const {
    return filename;
}
