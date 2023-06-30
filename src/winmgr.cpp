#include <stdexcept>

#include "winmgr.h"
#include "util.h"

#ifdef UI_X11
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#endif

typedef void (*t_glx_bind)(Display *, GLXDrawable, int , const int *);
typedef void (*t_glx_release)(Display *, GLXDrawable, int);

// manually bind to this function
static t_glx_bind glXBindTexImageEXT = 0;
static t_glx_release glXReleaseTexImageEXT = 0;

void WindowManager::_glfw_resize_callback(GLFWwindow* root_window, int width, int height)
{
    WindowManager* self = (WindowManager*) glfwGetWindowUserPointer(root_window);
    if (self->_draw_window) glfwSetWindowSize(self->_draw_window, width, height);
}

WindowManager::WindowManager(int _w, int _h, const char* name) : _can_composite(false), _width(_w), _height(_h)
{
    glfwWindowHint(GLFW_VISIBLE, 0); // hide first, show later when everything is ready
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, 1);
    _root_window = glfwCreateWindow(_width, _height, name, nullptr, nullptr);
    if (_root_window == nullptr)
        throw std::runtime_error("could not create GLFW window");
    
    glfwMakeContextCurrent(_root_window);
    glfwSwapInterval(1); // enable vsync
    glfwSetWindowUserPointer(_root_window, this);
    glfwSetWindowSizeCallback(_root_window, _glfw_resize_callback);

#ifdef UI_X11
    // use x11 composite extension to embed plugin UIs
    Display* xdisplay = glfwGetX11Display();

    int ext_major, ext_minor;
    if (XCompositeQueryExtension(xdisplay, &ext_major, &ext_minor))
    {
        int major_version, minor_version;
        _can_composite = XCompositeQueryVersion(xdisplay, &major_version, &minor_version)
            && (major_version > 0 || minor_version >= 2);
    }
    else
    {
        dbg("NOTICE: Xcomposite extension is not enabled, fallback to external plugin windows\n");
        return;
    }

    // also need XFixes to have an overlay window where ImGui is rendered
    int evt_base, err_base;
    if (!XFixesQueryExtension(xdisplay, &evt_base, &err_base)) {
        _can_composite = false;
        dbg("NOTICE: XFixes extension is not enabled, fallback to external plugin windows\n");
        return;
    }
    
    // bind to functions
    if (!glXBindTexImageEXT) glXBindTexImageEXT = (t_glx_bind) glXGetProcAddress((const GLubyte*)"glXBindTexImageEXT");
    if (!glXReleaseTexImageEXT) glXReleaseTexImageEXT = (t_glx_release) glXGetProcAddress((const GLubyte*)"glXReleaseTexImageEXT");

    if (!glXReleaseTexImageEXT || !glXReleaseTexImageEXT) {
        dbg("NOTICE: Could not find required GLX functions, fall back to external plugin windows\n");
        _can_composite = false;
    }

    // create draw window
    glfwWindowHint(GLFW_VISIBLE, 0);
    glfwWindowHint(GLFW_FLOATING, 1);
    _draw_window = glfwCreateWindow(_width, _height, name, nullptr, nullptr);
    if (!_draw_window)
        throw std::runtime_error("could not create GLFW window");
        
    glfwWindowHint(GLFW_FLOATING, 0);

    // parent draw window to root
    XReparentWindow(xdisplay, glfwGetX11Window(_draw_window), glfwGetX11Window(_root_window), 0, 0);
    XMapWindow(xdisplay, glfwGetX11Window(_draw_window));
#endif
}

WindowManager::~WindowManager() {
    if (_root_window) glfwDestroyWindow(_root_window);
    if (_draw_window) glfwDestroyWindow(_draw_window);
}

GLFWwindow* WindowManager::root_window() const {
    return _root_window;
}

GLFWwindow* WindowManager::draw_window() const {
    if (_draw_window == nullptr)
        return _root_window;
    else
     
        return _draw_window;
}