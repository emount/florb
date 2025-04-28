#include <X11/Xlib.h>
#include <GL/glew.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include "Florb.h"
#include <iostream>

Display* display;
Window window;
GLXContext context;

// Initialize OpenGL and X11 Window
void initOpenGL() {
    display = XOpenDisplay(nullptr);
    if (!display) throw std::runtime_error("Cannot open X11 display");

    int screen = DefaultScreen(display);

    static int visualAttribs[] = {
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        None
    };

    XVisualInfo* vi = glXChooseVisual(display, screen, visualAttribs);
    if (!vi) throw std::runtime_error("No appropriate visual found");

    Colormap cmap = XCreateColormap(display, RootWindow(display, vi->screen), vi->visual, AllocNone);

    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask;

    window = XCreateWindow(display, RootWindow(display, vi->screen),
                           0, 0, 800, 600, 0, vi->depth, InputOutput,
                           vi->visual, CWColormap | CWEventMask, &swa);
    if (!window) throw std::runtime_error("Failed to create window");

    XMapWindow(display, window);

    GLXContext tempContext = glXCreateContext(display, vi, nullptr, True);
    glXMakeCurrent(display, window, tempContext);

    typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = nullptr;
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
        glXGetProcAddress((const GLubyte*)"glXCreateContextAttribsARB");

    if (!glXCreateContextAttribsARB) {
        throw std::runtime_error("glXCreateContextAttribsARB not supported");
    }

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

    glXDestroyContext(display, tempContext);

    context = glXCreateContextAttribsARB(display, fbc[0], nullptr, True, context_attribs);
    if (!context) throw std::runtime_error("Failed to create modern GLX context");

    glXMakeCurrent(display, window, context);

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        throw std::runtime_error(std::string("GLEW init failed: ") + (const char*)glewGetErrorString(err));
    }
}

int main() {
    try {
        initOpenGL();

        glEnable(GL_DEPTH_TEST);

        Florb florb;
        florb.loadFlowers("images"); // Directory containing your flower images

        bool running = true;
        while (running) {
            while (XPending(display)) {
                XEvent event;
                XNextEvent(display, &event);
                if (event.type == KeyPress) {
                    running = false;
                }
            }

            florb.renderFrame();
            glXSwapBuffers(display, window);
        }

        glXMakeCurrent(display, None, nullptr);
        glXDestroyContext(display, context);
        XDestroyWindow(display, window);
        XCloseDisplay(display);

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
