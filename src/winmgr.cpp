#include <stdexcept>

#include "util.h"

#ifdef UI_X11
#include <glad/glad.h>
#include <X11/Xlib.h>

#ifdef COMPOSITING
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#endif

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#endif

#include <imgui.h>
#include <imgui_internal.h>
#include "winmgr.h"

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

#if defined(UI_X11) & defined(COMPOSITING)
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
    _draw_window = glfwCreateWindow(_width, _height, name, nullptr, _root_window);
    if (!_draw_window)
        throw std::runtime_error("could not create GLFW window");
        
    glfwWindowHint(GLFW_FLOATING, 0);

    // parent draw window to root
    XReparentWindow(xdisplay, glfwGetX11Window(_draw_window), glfwGetX11Window(_root_window), 0, 0);
    XMapWindow(xdisplay, glfwGetX11Window(_draw_window));
#endif
}

WindowManager::~WindowManager() {
    if (_draw_window) glfwDestroyWindow(_draw_window);
    if (_root_window) glfwDestroyWindow(_root_window);
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

bool WindowManager::is_item_hovered() const
{
    ImGuiContext& g = *GImGui;

    // assumes item isn't in a popup
    if (g.OpenPopupStack.Size > 0) return false;

    double xpos, ypos;
    glfwGetCursorPos(_root_window, &xpos, &ypos);

    ImVec2 tl = g.LastItemData.Rect.GetTL();
    ImVec2 br = g.LastItemData.Rect.GetBR();

    for (int i = g.Windows.Size - 1; i >= 0; i--) {
        ImGuiWindow* win = g.Windows[i];

        if (
            xpos > win->Pos.x && xpos < win->Pos.x + win->Size.x &&
            ypos > win->Pos.y && ypos < win->Pos.y + win->Size.y 
        ) {
            if (win == g.CurrentWindow)
                return xpos > tl.x && xpos < br.x &&
                       ypos > tl.y && ypos < br.y;
            else
                return false;
        }
    }
    
    return false;
}


#if defined(COMPOSITING) & defined(UI_X11)
WindowTexture::WindowTexture(Window wid)
:   _window(wid)
{
    Display* display = glfwGetX11Display();

    static const int pixmap_config[] = {
        GLX_BIND_TO_TEXTURE_RGB_EXT, True,
        GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT | GLX_WINDOW_BIT,
        GLX_BIND_TO_TEXTURE_TARGETS_EXT, GLX_TEXTURE_2D_BIT_EXT,
        //GLX_BIND_TO_MIPMAP_TEXTURE_EXT, True,
        GLX_BUFFER_SIZE, 24,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 0,
        0
    };

    static const int pixmap_attribs[] = {
        GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
        GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGB_EXT,
        //GLX_MIPMAP_TEXTURE_EXT, True,
        0
};

    // adapted from https://git.dec05eba.com/window-texture/tree/window_texture.c
    int result = 0;
    GLXFBConfig *configs = NULL;
    _pixmap = 0;
    _glx_pixmap = 0;
    _texture_id = 0;
    int glx_pixmap_bound = 0;

    XCompositeRedirectWindow(display, wid, CompositeRedirectAutomatic);

    XWindowAttributes attr;
    if (!XGetWindowAttributes(display, wid, &attr)) {
        dbg("ERROR: Failed to get X window attributes\n");
        return;
    }

    GLXFBConfig config;
    int c;

    // TODO: get proper screen?
    configs = glXChooseFBConfig(display, 0, pixmap_config, &c);
    if (!configs) {
        dbg("ERROR: Failed to choose FB config\n");
        return;
    }
    
    int found = 0;
    for (int i = 0; i < c; i++) {
        config = configs[i];
        XVisualInfo *visual = glXGetVisualFromFBConfig(display, config);
        if (!visual)
            continue;

        if (attr.depth != visual->depth) {
            XFree(visual);
            continue;
        }

        XFree(visual);
        found = 1;
        break;
    }

    if(!found) {
        dbg("ERROR: No matching fb config found\n");
        result = 1;
        goto cleanup;
    }

    _pixmap = XCompositeNameWindowPixmap(display, wid);
    if(!_pixmap) {
        result = 2;
        goto cleanup;
    }

    _glx_pixmap = glXCreatePixmap(display, config, _pixmap, pixmap_attribs);
    if(!_glx_pixmap) {
        result = 3;
        goto cleanup;
    }

    if(_texture_id == 0) {
        glGenTextures(1, &_texture_id);
        if(_texture_id == 0) {
            result = 4;
            goto cleanup;
        }
        glBindTexture(GL_TEXTURE_2D, _texture_id);
    } else {
        glBindTexture(GL_TEXTURE_2D, _texture_id);
    }

    glXBindTexImageEXT(display, _glx_pixmap, GLX_FRONT_EXT, NULL);
    glx_pixmap_bound = 1;

    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );

    
    //float fLargest = 0.0f;
    //glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, fLargest);
    

    glBindTexture(GL_TEXTURE_2D, 0);

    XFree(configs);
    _success = true;
    return;

    cleanup:
    if (_texture_id != 0)    glDeleteTextures(1, &_texture_id);
    if (_glx_pixmap)         glXDestroyPixmap(display, _glx_pixmap);
    if (glx_pixmap_bound)    glXReleaseTexImageEXT(display, _glx_pixmap, GLX_FRONT_EXT);
    if (_pixmap)             XFreePixmap(display, _pixmap);
    if (configs)             XFree(configs);
}

WindowTexture::~WindowTexture() {
    Display* xdisplay = glfwGetX11Display();
    XCompositeUnredirectWindow(xdisplay, _window, CompositeRedirectAutomatic);
            
    if (_texture_id) glDeleteTextures(1, &_texture_id);
    if (_glx_pixmap) {
        glXDestroyPixmap(xdisplay, _glx_pixmap);
        glXReleaseTexImageEXT(xdisplay, _glx_pixmap, GLX_FRONT_EXT);
    }
}
#endif

void WindowManager::update() {
    if (!_can_composite) return;

#if defined(UI_X11) & defined(COMPOSITING)
    // if a plugin windows is focused, make overlay
    // click-through
    Display* xdisplay = glfwGetX11Display();
    XRectangle rect = {};

    ImVec2 mp = ImGui::GetMousePos();

    if (!focused_window) {
        if (last_focused_window) {
            dbg("no active window\n");
            XRaiseWindow(xdisplay, glfwGetX11Window(_draw_window));
        }

        int w, h;
        glfwGetWindowSize(_root_window, &w, &h);
        rect.width = w;
        rect.height = h;
    } else {
        if (last_focused_window != focused_window) {
            dbg("new active window\n");
            XRaiseWindow(xdisplay, focused_window);
            XRaiseWindow(xdisplay, glfwGetX11Window(_draw_window));
        }
    }

    XserverRegion region = XFixesCreateRegion(xdisplay, &rect, 1);
    
    XFixesSetWindowShapeRegion(xdisplay, glfwGetX11Window(_draw_window), ShapeInput, 0, 0, region);
    XFixesDestroyRegion(xdisplay, region);

    last_focused_window = focused_window;
    focused_window = 0;
#endif
}