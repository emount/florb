// flower.cpp
#include <GL/glew.h>
#include <iostream>

#include "flower.h"
#include "florbUtils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using std::cerr;
using std::endl;

Flower::Flower(const std::string& filepath) {
    stbi_set_flip_vertically_on_load(true);
    int width, height, channels;
    unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &channels, 0);
    if (!data) {
        throw std::runtime_error("Failed to load image: " + filepath);
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
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
}

Flower::~Flower() {
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
}
