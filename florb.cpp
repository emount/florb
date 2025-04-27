// florb.cpp
// Modern OpenGL 4.6 + C++17 full-screen textured rotating quad demo

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

#include <cstring> // For memset

class FlorbApp {
public:
    FlorbApp();
    ~FlorbApp();
    void run();

private:
    // Window / GLX
    Display* display = nullptr;
    Window window = 0;
    GLXContext context = nullptr;
    int screen = 0;
    Atom wmDeleteMessage;

    // Timing
    std::chrono::steady_clock::time_point lastTime;

    // OpenGL
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLuint shaderProgram = 0;
    GLuint texture = 0;

    // Animation state
    float angle = 0.0f;

    // Methods
    void initWindow();
    void initGL();
    void createShaders();
    void createGeometry();
    void createTexture();
    void mainLoop();
    void renderFrame();
    void update();
    void handleEvents();
    void cleanup();

    // Helpers
    GLuint compileShader(GLenum type, const char* source);
};

const char* vertexShaderSource = R"glsl(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 projection;

out vec2 TexCoord;

void main()
{
    gl_Position = projection * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
)glsl";

const char* fragmentShaderSource = R"glsl(
#version 460 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D ourTexture;

void main()
{
    FragColor = texture(ourTexture, TexCoord);
}
)glsl";

FlorbApp::FlorbApp() {}

FlorbApp::~FlorbApp() {
    cleanup();
}

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

    XFree(vi);
    XFree(fbc);
}

void FlorbApp::initGL() {
    glViewport(0, 0, DisplayWidth(display, screen), DisplayHeight(display, screen));
    glEnable(GL_DEPTH_TEST);
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

void FlorbApp::createGeometry() {
    float vertices[] = {
        // positions         // texture coords
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,   1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f,   0.0f, 1.0f
    };

    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void FlorbApp::createTexture() {
    const int texWidth = 64;
    const int texHeight = 64;
    std::vector<unsigned char> textureData(texWidth * texHeight * 3);

    for (int y = 0; y < texHeight; ++y) {
        for (int x = 0; x < texWidth; ++x) {
            int checker = ((x / 8) % 2) ^ ((y / 8) % 2);
            unsigned char color = checker ? 255 : 50;
            textureData[(y * texWidth + x) * 3 + 0] = color;
            textureData[(y * texWidth + x) * 3 + 1] = color;
            textureData[(y * texWidth + x) * 3 + 2] = color;
        }
    }

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texWidth, texHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData.data());
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void FlorbApp::renderFrame() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shaderProgram);

    float rad = angle * 3.14159265f / 180.0f;
    float model[16] = {
        static_cast<float>(cos(rad)), static_cast<float>(-sin(rad)), 0.0f, 0.0f,
        static_cast<float>(sin(rad)), static_cast<float>(cos(rad)),  0.0f, 0.0f,
        0.0f,                         0.0f,                          1.0f, 0.0f,
        0.0f,                         0.0f,                          0.0f, 1.0f
    };

    int screenWidth = DisplayWidth(display, screen);
    int screenHeight = DisplayHeight(display, screen);
    float aspect = (float)screenWidth / (float)screenHeight;
    float projection[16] = {
        1.0f / aspect, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f,    0.0f, 0.0f,
        0.0f, 0.0f,   -1.0f, 0.0f,
        0.0f, 0.0f,    0.0f, 1.0f
    };

    GLuint modelLoc = glGetUniformLocation(shaderProgram, "model");
    GLuint projLoc = glGetUniformLocation(shaderProgram, "projection");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glUniform1i(glGetUniformLocation(shaderProgram, "ourTexture"), 0);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glXSwapBuffers(display, window);
}

void FlorbApp::update() {
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<float> deltaTime = now - lastTime;
    lastTime = now;

    angle += 45.0f * deltaTime.count();
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
        }
        else if (event.type == KeyPress) {
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
    if (texture) glDeleteTextures(1, &texture);

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
    createGeometry();
    createTexture();
    lastTime = std::chrono::steady_clock::now();
    mainLoop();
}

void FlorbApp::mainLoop() {
    while (true) {
        handleEvents();
        update();
        renderFrame();
    }
}

int main() {
    try {
        FlorbApp app;
        app.run();
    }
    catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return -1;
    }
    return 0;
}
