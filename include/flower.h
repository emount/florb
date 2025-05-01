// flower.h
#pragma once

#include <string>
#include <GL/gl.h>

class Flower {
public:
    Flower(const std::string& filename);
    ~Flower();

    GLuint getTextureID() const { return textureID; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getChannels() const { return channels; }
    GLenum getFormat() const { return format; }

private:
    GLuint textureID = 0;
    int width = 0;
    int height = 0;
    int channels = 0;
    GLenum format = 0;

    void loadFromFile(const std::string& filename);
};
