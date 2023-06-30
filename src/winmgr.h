#pragma once
// For functional plugin UI embedding, the application will
// need to manage native platform windows

#include <GLFW/glfw3.h>

#ifdef UI_X11
#include <GL/glx.h>
#include <GL/glext.h>
#endif

class WindowTexture
{
private:
    Pixmap _pixmap = 0;
    GLXPixmap _glx_pixmap = 0;
    GLuint _texture_id = 0;
    GLFWwindow* _window;
    int _width = 0;
    int _height = 0;
    bool _success = false;
public:
    WindowTexture(GLFWwindow* window);
    ~WindowTexture();

    inline bool is_loaded() const { return _success; };
    inline GLFWwindow* window() const { return _window; };
    inline GLuint texture_id() const { return _texture_id; };
};

class WindowManager
{
private:
    bool _can_composite = false;
    int _width, _height;

    GLFWwindow* _root_window = nullptr;

    // this window is always in front and is pass-through
    GLFWwindow* _draw_window = nullptr;
    // inbetween root_window and draw_window are the actual plugin windows

    static void _glfw_resize_callback(GLFWwindow* win, int w, int h);
    
public:
    WindowManager(int width, int height, const char* name);
    ~WindowManager();

    inline bool can_composite() const { return _can_composite; };

    GLFWwindow* root_window() const;
    GLFWwindow* draw_window() const;
} extern WindowCompositor;