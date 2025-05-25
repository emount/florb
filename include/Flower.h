#pragma once

#include <string>
#include <GL/gl.h>

class Flower {
public:
    Flower(const std::string& filename);
    ~Flower();

    void loadImage();

    const std::string& getFilename() const;
  
    GLuint getTextureID() const;

    GLint getWidth() const;
    
    GLint getHeight() const;
    
    GLint getChannels() const;
    
    GLenum getFormat() const;

    Flower& operator=(const Flower& other);

private:
    std::string filename;
  
    GLuint textureID = 0;
    GLint width = 0;
    GLint height = 0;
    GLint channels = 0;
    GLenum format = 0;

    void loadFromFile(const std::string& filename);
};
