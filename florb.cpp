#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <GL/glew.h>
#include <GL/glu.h>

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace fs = std::filesystem;

// Settings
const int WINDOW_WIDTH = 1200;
const int WINDOW_HEIGHT = 800;
const float BASE_RADIUS = 2.0f;
const float PULSE_AMPLITUDE = 0.4f;
const int MORPH_INTERVAL_MS = 60000;

// Globals
std::vector<GLuint> textures;
int currentTexture = 0;
int nextTexture = 1;
Uint32 lastMorphTime = 0;

float volume = 1.0f;
bool muted = false;

Mix_Chunk* music = nullptr;

// OpenGL Helpers
GLuint loadTexture(const std::string& path) {
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!data) {
        std::cerr << "Failed to load image: " << path << std::endl;
        return 0;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    return tex;
}

void drawSphere(float radius) {
    GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    gluQuadricNormals(quad, GLU_SMOOTH);
    gluSphere(quad, radius, 64, 64);
    gluDeleteQuadric(quad);
}

float getAudioLevel() {
    // Placeholder: no real RMS calculation yet
    // Would normally read audio buffer here
    return 0.5f;  // Simulate constant mid pulse for now
}

void setupOpenGL() {
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0, 0, 0, 1);
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL Init Failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow("Flower Orb",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          WINDOW_WIDTH, WINDOW_HEIGHT,
                                          SDL_WINDOW_OPENGL);

    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    glewInit();
    setupOpenGL();

    // Load textures
    for (const auto& entry : fs::directory_iterator("flowers")) {
        if (entry.is_regular_file()) {
            textures.push_back(loadTexture(entry.path().string()));
        }
    }
    if (textures.empty()) {
        std::cerr << "No flower textures found." << std::endl;
        return -1;
    }

    // Load music
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    music = Mix_LoadWAV("audio/music.wav");  // Use WAV/OGG for now
    if (music) {
        Mix_PlayChannel(-1, music, -1);
    }

    lastMorphTime = SDL_GetTicks();

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_m) {
                    muted = !muted;
                    Mix_Volume(-1, muted ? 0 : int(volume * MIX_MAX_VOLUME));
                }
                if (event.key.keysym.sym == SDLK_UP) {
                    volume = std::min(1.0f, volume + 0.1f);
                    Mix_Volume(-1, int(volume * MIX_MAX_VOLUME));
                }
                if (event.key.keysym.sym == SDLK_DOWN) {
                    volume = std::max(0.0f, volume - 0.1f);
                    Mix_Volume(-1, int(volume * MIX_MAX_VOLUME));
                }
            }
        }

        Uint32 now = SDL_GetTicks();
        if (now - lastMorphTime > MORPH_INTERVAL_MS) {
            currentTexture = nextTexture;
            nextTexture = rand() % textures.size();
            lastMorphTime = now;
        }

        float alpha = (now - lastMorphTime) / float(MORPH_INTERVAL_MS);
        float pulse = getAudioLevel();
        float sphereRadius = BASE_RADIUS + PULSE_AMPLITUDE * pulse;

        // Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -6.0f);
        glRotatef(now / 50.0f, 0.0f, 1.0f, 0.0f);
        glRotatef(now / 70.0f, 1.0f, 0.0f, 0.0f);

        // Blending current and next textures manually (simple)
        glBindTexture(GL_TEXTURE_2D, textures[currentTexture]);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f - alpha);
        drawSphere(sphereRadius);

        glBindTexture(GL_TEXTURE_2D, textures[nextTexture]);
        glColor4f(1.0f, 1.0f, 1.0f, alpha);
        drawSphere(sphereRadius);

        SDL_GL_SwapWindow(window);
        SDL_Delay(16);  // ~60 FPS
    }

    for (auto tex : textures) {
        glDeleteTextures(1, &tex);
    }

    if (music)
        Mix_FreeChunk(music);

    Mix_CloseAudio();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
