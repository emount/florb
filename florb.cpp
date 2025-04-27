// florb.cpp
// Modern OpenGL 4.6 + C++17 full-screen semi-transparent 3D sphere

#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <stdexcept>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/glext.h>

#include <cstring>

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

    void initWindow();
    void initGL();
    void createShaders();
    void createSphereGeometry();
    void mainLoop();
    void renderFrame();
    void handleEvents();
    void cleanup();

    GLuint compileShader(GLenum type, const char* source);
};

const char* vertexShaderSource = R"glsl(
#version 460 core
layout(location = 0) in vec3 aPos;
out vec3 fragPos;

uniform mat4 projection;

void main()
{
    fragPos = aPos;
    gl_Position = projection * vec4(aPos, 1.0);
}
)glsl";

const char* fragmentShaderSource = R"glsl(
#version 460 core
out vec4 FragColor;

in vec3 fragPos;

void main()
{
    float intensity = 1.0 - length(fragPos);
    intensity = clamp(intensity, 0.0, 1.0);
    FragColor = vec4(vec3(1.0), 0.5 * intensity);
}
)glsl";

FlorbApp::FlorbApp() {}
FlorbApp::~FlorbApp() { cleanup(); }

GLuint FlorbApp::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        throw std::runtime_error(std::string("Shader compile error: ") + infoLog);
    }

    return shader;
}

void FlorbApp::initWindow() {
    display = XOpenDisplay(nullptr);
    if (!display)
        throw std::runtime_error("Failed to open X display");

    screen = DefaultScreen(display);

    static int visualAttribs[] = {
        GLX_RENDER_TYPE,   GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_DOUBLEBUFFER,  True,
        GLX_RED_SIZE,      8,
        GLX_GREEN_SIZE,    8,
        GLX_BLUE_SIZE,     8,
        GLX_ALPHA_SIZE,    8,
        GLX_DEPTH_SIZE,    24,
        None
    };

    int fbcount = 0;
    GLXFBConfig* fbc = glXChooseFBConfig(display, screen, visualAttribs, &fbcount);
    if (!fbc)
        throw std::runtime_error("Failed to get FBConfig");

    XVisualInfo* vi = glXGetVisualFromFBConfig(display, fbc[0]);
    if (!vi)
        throw std::runtime_error("No appropriate visual found");

    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(display, RootWindow(display, vi->screen), vi->visual, AllocNone);
    swa.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;

    int screenWidth = DisplayWidth(display, screen);
    int screenHeight = DisplayHeight(display, screen);

    window = XCreateWindow(display, RootWindow(display, vi->screen),
                           0, 0, screenWidth, screenHeight, 0,
                           vi->depth, InputOutput,
                           vi->visual,
                           CWColormap | CWEventMask, &swa);

    if (!window)
        throw std::runtime_error("Failed to create window");

    Atom wmState = XInternAtom(display, "_NET_WM_STATE", False);
    Atom fullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
    XChangeProperty(display, window, wmState, XA_ATOM, 32,
                    PropModeReplace, (unsigned char*)&fullscreen, 1);

    wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wmDeleteMessage, 1);

    XMapWindow(display, window);
    XFlush(display);

    context = glXCreateNewContext(display, fbc[0], GLX_RGBA_TYPE, nullptr, True);
    if (!context)
        throw std::runtime_error("Failed to create GL context");

    glXMakeCurrent(display, window, context);

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "glewInit() returned error: (" << err << ")" << std::endl;
        exit(-1);
    }

    XFree(vi);
    XFree(fbc);
}

void FlorbApp::initGL() {
    glViewport(0, 0, DisplayWidth(display, screen), DisplayHeight(display, screen));
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void FlorbApp::createShaders() {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int success = 0;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        throw std::runtime_error(std::string("Program link error: ") + infoLog);
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

void FlorbApp::renderFrame() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shaderProgram);

    int screenWidth = DisplayWidth(display, screen);
    int screenHeight = DisplayHeight(display, screen);
    float aspect = (float)screenWidth / (float)screenHeight;
    float projection[16] = {
        1.0f / aspect, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f,    0.0f, 0.0f,
        0.0f, 0.0f,   -1.0f, 0.0f,
        0.0f, 0.0f,    0.0f, 1.0f
    };

    GLuint projLoc = glGetUniformLocation(shaderProgram, "projection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection);

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
    initGL();
    createShaders();
    createSphereGeometry();
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
