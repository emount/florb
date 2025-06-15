#include <X11/Xlib.h>
#include <GL/glew.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

#include "Florb.h"
#include "FlorbConfigs.h"
#include "FlorbUtils.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 4

namespace chrono = std::chrono;

using chrono::duration;
using chrono::duration_cast;
using chrono::milliseconds;
using chrono::steady_clock;

using std::cerr;
using std::cout;
using std::endl;
using std::exception;
using std::runtime_error;
using std::string;

namespace this_thread = std::this_thread;


Display *display;
Window window;
GLXContext context;

int screenWidth = 800;
int screenHeight = 600;


void generateFramebuffer() {
    GLuint fbo1, tex1;
    glGenFramebuffers(1, &fbo1);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo1);
    
    // Create color texture to attach
    glGenTextures(1, &tex1);
    glBindTexture(GL_TEXTURE_2D, tex1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screenWidth, screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);
    
    // Create an optional depth buffer
    GLuint rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, screenWidth, screenHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
    
    // Check FBO status
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        cerr << "Framebuffer is not complete" << endl;
    
     glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


// Initialize OpenGL and X11 Window
void initOpenGL() {
    display = XOpenDisplay(nullptr);
    if (!display) throw runtime_error("Cannot open X11 display");

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
    if (!vi) throw runtime_error("No appropriate visual found");

    Colormap cmap = XCreateColormap(display, RootWindow(display, vi->screen), vi->visual, AllocNone);

    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;

    window = XCreateWindow(display, RootWindow(display, vi->screen),
                           0, 0, screenWidth, screenHeight, 0, vi->depth, InputOutput,
                           vi->visual, CWColormap | CWEventMask, &swa);
    if (!window) throw runtime_error("Failed to create window");

    // Map the window (show it)
    XMapWindow(display, window);
    XFlush(display);


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
        throw runtime_error("glXCreateContextAttribsARB not supported");
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
    if (!fbc) throw runtime_error("Failed to get FBConfig");

    glXDestroyContext(display, tempContext);

    context = glXCreateContextAttribsARB(display, fbc[0], nullptr, True, context_attribs);
    if (!context) throw runtime_error("Failed to create modern GLX context");

    glXMakeCurrent(display, window, context);

    
    // Disable VSYNC, permitting arbitrary frame rates
    typedef void (*glXSwapIntervalEXTProc)(Display*, GLXDrawable, int);
    glXSwapIntervalEXTProc glXSwapIntervalEXT = 
      (glXSwapIntervalEXTProc)glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalEXT");
    if (glXSwapIntervalEXT) {
        glXSwapIntervalEXT(display, glXGetCurrentDrawable(), 0);
    }


    // Generate an intermediate framebuffer for fragment shader pipelining
    //generateFramebuffer();

    // Generate a test texture for use in debugging
    GLint testTex = 0;
    glGenTextures(1, (GLuint*)&testTex);

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        throw runtime_error(string("GLEW init failed: ") + (const char*)glewGetErrorString(err));
    }

    // Graphical housekeeping
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);  // Wireframe view

    
    const GLubyte* version = glGetString(GL_VERSION);
    cout << "[INFO] OpenGL version: " << version << endl;
}

int main(int numArgs, const char *args[]) {

    cout << "Florb v" << VERSION_MAJOR << "." << VERSION_MINOR << endl;

    // All parameters are provided by the config JSON
    if (numArgs > 1) {
        cerr << "Usage : " << args[0] << endl;
        return -EINVAL;
    }

    try {
        initOpenGL();

        glEnable(GL_DEPTH_TEST);

        Florb florb;
        
        bool running = true;
        while (running) {
            while (XPending(display)) {
                XEvent event;
                XNextEvent(display, &event);

                if (event.type == KeyPress) {
                    KeySym sym = XLookupKeysym(&event.xkey, 0);
                    
                    if (sym == XK_space) {
                        cout << "Showing the dashboard" << endl;
                    } else if (sym == XK_Escape) {
                        // Exit the application
                        cout << "Exiting Florb" << endl;
                        running = false;
                    }
                }
                else if (event.type == ConfigureNotify) {
                    XConfigureEvent xce = event.xconfigure;
                    if (xce.width != screenWidth || xce.height != screenHeight) {
                        screenWidth = xce.width;
                        screenHeight = xce.height;
                        glViewport(0, 0, screenWidth, screenHeight);
                        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    }
                }
            }

            // Get a pointer to the Florb configs
            auto florbConfigs(florb.getConfigs());
            
            // Update timing for flower transition
            static steady_clock::time_point startTime = steady_clock::now();
            float timeMsec = duration_cast<milliseconds>(steady_clock::now() - startTime).count();
            float timeSeconds = (timeMsec / 1000.0f);

            bool transition(false);
            if (timeSeconds >= florbConfigs->getImageSwitch()) {
                florb.nextFlower();
                startTime = steady_clock::now();
                transition = true;
            }

            // Fetch the configurable frame rate from the Florb
            const float k_FrameTime(1000.0f / florbConfigs->getVideoFrameRate());

            static steady_clock::time_point lastFrame = steady_clock::now();
            float frameElapsed = duration_cast<milliseconds>(steady_clock::now() - lastFrame).count();

            if (frameElapsed < k_FrameTime) {
              // Busy-wait or sleep for the remaining time
              this_thread::sleep_for(milliseconds(static_cast<int>(k_FrameTime - frameElapsed)));
            }

            // Mark start of new frame
            lastFrame = steady_clock::now();
 
            // Render a flower frame, marking transition frames
            florb.renderFrame(transition);
            glXSwapBuffers(display, window);


        // cerr << "Frame start : "
        //          << duration_cast<milliseconds>(frameElapsed).count() << " ms" << endl;
            
        } // while (running)

        glXMakeCurrent(display, None, nullptr);

        glXDestroyContext(display, context);
        XDestroyWindow(display, window);
        XCloseDisplay(display);

    } catch (const exception& e) {
        cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
