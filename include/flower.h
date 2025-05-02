// flower.h
#pragma once

#include <string>
#include <GL/gl.h>

class Flower {
public:
    Flower(const std::string& filename);
    ~Flower();

    void loadImage();

    const std::string& getFilename() const;
    GLuint getTextureID() const { return textureID; }
    GLint getWidth() const { return width; }
    GLint getHeight() const { return height; }
    GLint getChannels() const { return channels; }
    GLenum getFormat() const { return format; }

private:
    const std::string filename;
  
    GLuint textureID = 0;
    GLint width = 0;
    GLint height = 0;
    GLint channels = 0;
    GLenum format = 0;

    void loadFromFile(const std::string& filename);
};
