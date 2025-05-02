#include <X11/Xlib.h>
#include <GL/glew.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <chrono>
#include <iostream>
#include <thread>

#include "florb.h"

Display* display;
Window window;
GLXContext context;
int screenWidth = 800;
int screenHeight = 600;

using std::cerr;
using std::endl;

// Initialize OpenGL and X11 Window
void initOpenGL() {
    display = XOpenDisplay(nullptr);
    if (!display) throw std::runtime_error("Cannot open X11 display");

    int screenNum = DefaultScreen(display);
    Screen* screen = ScreenOfDisplay(display, screenNum);

    screenWidth = screen->width;
    screenHeight = screen->height;

    static int visualAttribs[] = {
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        None
    };

    XVisualInfo* vi = glXChooseVisual(display, screenNum, visualAttribs);
    if (!vi) throw std::runtime_error("No appropriate visual found");

    Colormap cmap = XCreateColormap(display, RootWindow(display, vi->screen), vi->visual, AllocNone);

    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;

    window = XCreateWindow(display, RootWindow(display, vi->screen),
                           0, 0, screenWidth, screenHeight, 0, vi->depth, InputOutput,
                           vi->visual, CWColormap | CWEventMask, &swa);
    if (!window) throw std::runtime_error("Failed to create window");

    // Map the window (show it)
    XMapWindow(display, window);

    // Request FULLSCREEN from window manager
    Atom wmState = XInternAtom(display, "_NET_WM_STATE", False);
    Atom fullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);

    XEvent xev = {};
    xev.type = ClientMessage;
    xev.xclient.window = window;
    xev.xclient.message_type = wmState;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
    xev.xclient.data.l[1] = fullscreen;
    xev.xclient.data.l[2] = 0; // no second property to toggle
    xev.xclient.data.l[3] = 1; // normal application request
    xev.xclient.data.l[4] = 0;

    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xev);

    // Temporary GLX context to load extensions
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
    fbc = glXChooseFBConfig(display, screenNum, fbAttribs, &fbcount);
    if (!fbc) throw std::runtime_error("Failed to get FBConfig");

    glXDestroyContext(display, tempContext);

    context = glXCreateContextAttribsARB(display, fbc[0], nullptr, True, context_attribs);
    if (!context) throw std::runtime_error("Failed to create modern GLX context");

    glXMakeCurrent(display, window, context);

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        throw std::runtime_error(std::string("GLEW init failed: ") + (const char*)glewGetErrorString(err));
    }

    // Graphical housekeeping
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    const GLubyte* version = glGetString(GL_VERSION);
    cerr << "[INFO] OpenGL version: " << version << endl;
}

int main() {
    const int TARGET_FPS = 60;
    const int FRAME_DURATION_MS = 1000 / TARGET_FPS;

    try {
        initOpenGL();

        glEnable(GL_DEPTH_TEST);

        Florb florb;
        bool running = true;
        while (running) {
            auto frameStart = std::chrono::high_resolution_clock::now();
        
            while (XPending(display)) {
                XEvent event;
                XNextEvent(display, &event);
        
                if (event.type == KeyPress) {
                    KeySym sym = XLookupKeysym(&event.xkey, 0);
                    if (sym == XK_Escape || sym == XK_space)
		        running = false;
                }
                else if (event.type == ConfigureNotify) {
                    XConfigureEvent xce = event.xconfigure;
                    if (xce.width != screenWidth || xce.height != screenHeight) {
                        screenWidth = xce.width;
                        screenHeight = xce.height;
                        glViewport(0, 0, screenWidth, screenHeight);
                    }
                }
            }
        
            florb.renderFrame();
            glXSwapBuffers(display, window);
        
            auto frameEnd = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float, std::milli> elapsed = frameEnd - frameStart;
        
            if (elapsed.count() < FRAME_DURATION_MS) {
                std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DURATION_MS) - elapsed);
            }
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
