// flower.cpp
#include <GL/glew.h>
#include <iostream>

#include "Flower.h"
#include "FlorbUtils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// #define DEBUG_MESSAGES

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

    // Discard any existing texture
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
    
    glGenTextures(1, &textureID);

    glBindTexture(GL_TEXTURE_2D, textureID);
    FlorbUtils::glCheck("glBindTexture()");

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    FlorbUtils::glCheck("glTexImage2D()");

    GLint texWidth = 0, texHeight = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texHeight);
    
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);

#ifdef DEBUG_MESSAGES
    cerr << "Loaded flower image \""
         << getFilename()
         << "\""
         << endl;
#endif
}

const string& Flower::getFilename() const {
    return filename;
}
  
GLuint Flower::getTextureID() const {
#ifdef DEBUG_MESSAGES
    cerr << "Returning texture ID ("
         << textureID
         << ") for flower image \""
         << getFilename()
         << "\""
         << endl;
#endif
    return textureID;
}

GLint Flower::getWidth() const {
    return width;
}
    
GLint Flower::getHeight() const {
    return height;
}
    
GLint Flower::getChannels() const {
    return channels;
}
    
GLenum Flower::getFormat() const {
    return format;
}

Flower& Flower::operator=(const Flower& other) {
    if (this != &other) {
        filename = other.filename;

        // If there's an existing texture, delete it
        if (textureID != 0) {
            glDeleteTextures(1, &textureID);
            textureID = 0;

            // Re-load texture from filename if available
            if (!filename.empty()) {
                try {
                    loadImage();  // this sets textureID
                } catch (const std::exception& e) {
                    cerr << "Failed to assign Flower: " << e.what() << endl;
                }
            }
        }
    }

    return *this;
}
