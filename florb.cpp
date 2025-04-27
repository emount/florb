// florb.cpp
// Full application - projects images onto inside of a 3D sphere

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <cmath>
#include <chrono>
#include <stdexcept>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/glext.h>

namespace fs = std::filesystem;

Display* display = nullptr;
Window window = 0;
GLXContext context = 0;

// Flower class
class Flower {
public:
    GLuint textureID = 0;

    Flower(const std::string& filepath) {
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
    
        unsigned char data[] = {
            255,   0,   0, 255,   0, 255,   0, 255,
              0,   0, 255, 255, 255, 255,   0, 255
        }; // 2x2 checker pattern
    
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
        // Important: generate mipmaps
        glGenerateMipmap(GL_TEXTURE_2D);
    
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // Now valid
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
  
//    Flower(const std::string& filepath) {
//        int width, height, channels;
//        stbi_set_flip_vertically_on_load(true); // Flip image on load to match OpenGL UVs
//    
//        unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &channels, 4);
//        if (!data) {
//            throw std::runtime_error("Failed to load image: " + filepath);
//        }
//    
//        glGenTextures(1, &textureID);
//        glBindTexture(GL_TEXTURE_2D, textureID);
//    
//        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
//    
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//    
//        stbi_image_free(data);
//    }

    ~Flower() {
        if (textureID) glDeleteTextures(1, &textureID);
    }
};

// FlorbApp class
class FlorbApp {
public:
    FlorbApp();
    ~FlorbApp();
    void run();

private:
    Display* display = nullptr;
    Window window = 0;
    GLXContext context = nullptr;
    int screen = 0;
    Atom wmDeleteMessage;

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLuint shaderProgram = 0;
    size_t indexCount = 0;

    std::vector<Flower> flowers;
    size_t currentFlower = 0;
    size_t nextFlower = 1;

    std::chrono::steady_clock::time_point lastSwitch;

    void initWindow();
    void initOpenGL();
    void createShaders();
    void createSphereGeometry();
    void loadFlowers();
    void mainLoop();
    void renderFrame();
    void handleEvents();
    void cleanup();

    GLuint compileShader(GLenum type, const char* source);
};

// Shaders
const char* vertexShaderSource = R"glsl(
#version 460 core
layout(location = 0) in vec3 aPos;
out vec3 fragPos;

uniform mat4 projection;

void main()
{
    fragPos = normalize(aPos);
    gl_Position = projection * vec4(aPos, 1.0);
}
)glsl";

const char* fragmentShaderSource = R"glsl(
#version 460 core
out vec4 FragColor;

uniform sampler2D currentTexture;

void main()
{
    FragColor = texture(currentTexture, vec2(0.5, 0.5));
}
// #version 460 core
// out vec4 FragColor;
// in vec3 fragPos;
// 
// uniform sampler2D currentTexture;
// 
// void main()
// {
//     vec3 dir = normalize(fragPos);
//     vec2 uv;
//     uv.x = atan(dir.z, dir.x) / (2.0 * 3.14159265) + 0.5;
//     uv.y = asin(dir.y) / 3.14159265 + 0.5;
//     uv = clamp(uv, 0.0, 1.0);
//     uv.y = 1.0 - uv.y; // Flip vertically
// 
//     FragColor = texture(currentTexture, uv);
// }
)glsl";

// Implementation
FlorbApp::FlorbApp() {}
FlorbApp::~FlorbApp() { cleanup(); }

GLuint FlorbApp::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        throw std::runtime_error("Shader compile error: " + std::string(infoLog));
    }
    return shader;
}

void FlorbApp::initWindow() {
    display = XOpenDisplay(nullptr);
    if (!display) throw std::runtime_error("Failed to open X display");

    screen = DefaultScreen(display);

    static int visualAttribs[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_DOUBLEBUFFER, True,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        None
    };

    int fbcount;
    GLXFBConfig* fbc = glXChooseFBConfig(display, screen, visualAttribs, &fbcount);
    if (!fbc) throw std::runtime_error("Failed to get FBConfig");

    XVisualInfo* vi = glXGetVisualFromFBConfig(display, fbc[0]);
    if (!vi) throw std::runtime_error("No appropriate visual found");

    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(display, RootWindow(display, vi->screen), vi->visual, AllocNone);
    swa.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;

    int width = DisplayWidth(display, screen);
    int height = DisplayHeight(display, screen);

    window = XCreateWindow(display, RootWindow(display, vi->screen),
                           0, 0, width, height, 0, vi->depth, InputOutput,
                           vi->visual, CWColormap | CWEventMask, &swa);

    if (!window) throw std::runtime_error("Failed to create window");

    Atom wmState = XInternAtom(display, "_NET_WM_STATE", False);
    Atom fullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
    XChangeProperty(display, window, wmState, XA_ATOM, 32,
                    PropModeReplace, (unsigned char*)&fullscreen, 1);

    wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wmDeleteMessage, 1);

    XMapWindow(display, window);
    XFlush(display);

    context = glXCreateNewContext(display, fbc[0], GLX_RGBA_TYPE, nullptr, True);
    if (!context) throw std::runtime_error("Failed to create GL context");

    glXMakeCurrent(display, window, context);

    GLenum err = glewInit();
    if (err != GLEW_OK) throw std::runtime_error("GLEW initialization failed");

    XFree(vi);
    XFree(fbc);
}

#include <cstring> // Needed for memset

void FlorbApp::initOpenGL() {
    display = XOpenDisplay(nullptr);
    if (!display) throw std::runtime_error("Cannot open X11 display");
    std::cout << "XOpenDisplay OK" << std::endl;

    int screen = DefaultScreen(display);

    static int visualAttribs[] = {
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        None
    };

    XVisualInfo* vi = glXChooseVisual(display, screen, visualAttribs);
    if (!vi) throw std::runtime_error("No appropriate visual found");
    std::cout << "glXChooseVisual OK" << std::endl;

    Colormap cmap = XCreateColormap(display, RootWindow(display, vi->screen), vi->visual, AllocNone);

    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask;

    window = XCreateWindow(display, RootWindow(display, vi->screen),
                           0, 0, 800, 600, 0, vi->depth, InputOutput,
                           vi->visual, CWColormap | CWEventMask, &swa);
    if (!window) throw std::runtime_error("Failed to create window");
    std::cout << "Window creation OK" << std::endl;

    XMapWindow(display, window);

    // Temporary context to load modern GLX functions
    GLXContext tempContext = glXCreateContext(display, vi, nullptr, True);
    glXMakeCurrent(display, window, tempContext);

    typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = nullptr;
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
        glXGetProcAddress((const GLubyte*)"glXCreateContextAttribsARB");

    if (!glXCreateContextAttribsARB) {
        throw std::runtime_error("glXCreateContextAttribsARB not supported");
    }
    std::cout << "glXCreateContextAttribsARB found OK" << std::endl;

    int context_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        None
    };

    GLXFBConfig* fbc = nullptr;
    int fbcount;
    int fbAttribs[] = {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER, True,
        None
    };
    fbc = glXChooseFBConfig(display, screen, fbAttribs, &fbcount);
    if (!fbc) throw std::runtime_error("Failed to get FBConfig");

    glXDestroyContext(display, tempContext); // Kill old temporary context

    context = glXCreateContextAttribsARB(display, fbc[0], nullptr, True, context_attribs);
    if (!context) throw std::runtime_error("Failed to create modern GLX context");

    glXMakeCurrent(display, window, context);
    std::cout << "Modern OpenGL context created OK" << std::endl;

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        throw std::runtime_error(std::string("GLEW init failed: ") + (const char*)glewGetErrorString(err));
    }
    std::cout << "GLEW initialized OK" << std::endl;
}

void FlorbApp::createShaders() {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        throw std::runtime_error("Shader link error: " + std::string(infoLog));
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void FlorbApp::createSphereGeometry() {
    const int sectorCount = 64;
    const int stackCount = 64;
    const float radius = 0.8f;

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    for (int i = 0; i <= stackCount; ++i) {
        float stackAngle = M_PI / 2 - i * M_PI / stackCount;
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);

        for (int j = 0; j <= sectorCount; ++j) {
            float sectorAngle = j * 2 * M_PI / sectorCount;
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
        }
    }

    for (int i = 0; i < stackCount; ++i) {
        int k1 = i * (sectorCount + 1);
        int k2 = k1 + sectorCount + 1;

        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            if (i != (stackCount - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    indexCount = indices.size();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

void FlorbApp::loadFlowers() {
    for (const auto& entry : fs::directory_iterator("images")) {
        if (entry.is_regular_file()) {
            try {
                flowers.emplace_back(entry.path().string());
            } catch (...) {
                std::cerr << "Failed to load " << entry.path() << std::endl;
            }
        }
    }
    if (flowers.empty()) throw std::runtime_error("No images found in images/");
}

void FlorbApp::renderFrame() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shaderProgram);

    int width = DisplayWidth(display, screen);
    int height = DisplayHeight(display, screen);
    float aspect = (float)width / (float)height;

    float projection[16] = {
        1.0f / aspect, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f,    0.0f, 0.0f,
        0.0f, 0.0f,   -1.0f, 0.0f,
        0.0f, 0.0f,    0.0f, 1.0f
    };

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, projection);

    // Critical correct binding:
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, flowers[currentFlower].textureID);
    glUniform1i(glGetUniformLocation(shaderProgram, "currentTexture"), 0); // bind to texture unit 0

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);

    glXSwapBuffers(display, window);
}

void FlorbApp::handleEvents() {
    while (XPending(display)) {
        XEvent event;
        XNextEvent(display, &event);

        if (event.type == ClientMessage) {
            if (event.xclient.data.l[0] == (long)wmDeleteMessage) {
                cleanup();
                exit(0);
            }
        } else if (event.type == KeyPress) {
            cleanup();
            exit(0);
        }
    }
}

void FlorbApp::cleanup() {
    if (shaderProgram) glDeleteProgram(shaderProgram);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);

    if (context) {
        glXMakeCurrent(display, None, nullptr);
        glXDestroyContext(display, context);
    }

    if (window) {
        XDestroyWindow(display, window);
    }

    if (display) {
        XCloseDisplay(display);
    }
}

void FlorbApp::run() {
    initWindow();
    initOpenGL();
    createShaders();
    createSphereGeometry();
    loadFlowers();
    lastSwitch = std::chrono::steady_clock::now();
    mainLoop();
}

void FlorbApp::mainLoop() {
    while (true) {
        handleEvents();
        renderFrame();
    }
}

int main() {
    try {
        FlorbApp app;
        app.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return -1;
    }
    return 0;
}
