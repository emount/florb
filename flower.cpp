// flower.cpp
#include <GL/glew.h>
#include <iostream>

#include "flower.h"
#include "florbUtils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using std::cerr;
using std::endl;
using std::string;

Flower::Flower(const std::string& filename) :
    filename(filename) { }

Flower::~Flower() {
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
}

void Flower::loadImage() {
    stbi_set_flip_vertically_on_load(false);
    int width, height, channels;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 0);
    
    if (!data) {
        throw std::runtime_error("Failed to load image: " + filename);
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
    
    glGenTextures(1, &textureID);

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

    glBindTexture(GL_TEXTURE_2D, textureID);
    FlorbUtils::glCheck("glBindTexture()");
		    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    FlorbUtils::glCheck("glTexImage2D()");

    GLint texWidth = 0, texHeight = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texHeight);
    std::cerr << "Bound texture ID "
	      << textureID
	      << " has dimensions ["
	      << texWidth
	      << "x"
	      << texHeight
	      << "]"
	      << endl;
    
    glGenerateMipmap(GL_TEXTURE_2D);

    // glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
}

const string& Flower::getFilename() const {
    return filename;
}
