#include "lv2interface.h"
#include "lv2internal.h"
#include "../../util.h"
#include <lv2/ui/ui.h>
#include <suil/suil.h>
#include <lv2/instance-access/instance-access.h>
#include <cstdlib>
#include <imgui.h>
#include <chrono>

#ifdef UI_X11
#include <X11/Xlib.h>
#undef None // ...
#elif defined(UI_WINDOWS)
#include <windows.h>
#endif

#ifdef UI_X11
#define GLFW_EXPOSE_NATIVE_X11
#elif defined(UI_WINDOWS)
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

using namespace lv2;

static uint32_t suil_port_index_func(SuilController controller, const char* port_symbol)
{
    abort();
}

static uint32_t suil_port_subscribe_func(
    SuilController controller,
    uint32_t port_index,
    uint32_t protocol,
    const LV2_Feature* const* features
) {
    abort();
}

static uint32_t suil_port_unsubscribe_func(
    SuilController controller,
    uint32_t port_index,
    uint32_t protocol,
    const LV2_Feature* const* features
) {
    abort();
}

static void suil_port_write_func(
    SuilController controller,
    uint32_t port_index,
    uint32_t buffer_size,
    uint32_t protocol,
    void const* buffer
) {
    abort();
}

void suil_touch_func(
    SuilController controller,
    uint32_t port_index,
    bool grabbed
) {
    abort();
}

void __touch(LV2UI_Feature_Handle handle, uint32_t port_index, bool grabbed) {
    dbg("touch\n");
}

UIHost::UIHost() {}

void UIHost::init(plugins::Lv2Plugin* __plugin_controller)
{
    const LilvNode* host_type =
    #ifdef UI_WINDOWS
        URI.ui_WindowsUI;
    #else
        URI.ui_X11UI;
    #endif

    plugin_ctl = __plugin_controller;

    LilvUIs* uis = lilv_plugin_get_uis(plugin_ctl->lv2_plugin_data);
    
    if (uis)
    {
        const LilvUI* target_ui_n = nullptr;
        const char* target_ui_type = nullptr;
        unsigned min_support = 999999; // some arbitrarily high number

        // find the ui widget that is most supported by suil
        LILV_FOREACH (uis, i, uis)
        {
            const LilvUI* ui_n = lilv_uis_get(uis, i);

            const LilvNode* ui_type_n;

            unsigned support = lilv_ui_is_supported(ui_n, suil_ui_supported, host_type, &ui_type_n);

            if (support == 0) continue;
            if (support == 1) {
                target_ui_n = ui_n;
                target_ui_type = lilv_node_as_uri(ui_type_n);
                min_support = 1;
                break;
            } else {
                if (support < min_support) {
                    min_support = support;
                    target_ui_n = ui_n;
                    target_ui_type = lilv_node_as_uri(ui_type_n);
                }
            }
        }

        if (target_ui_n)
        {
            dbg("found supported ui %s\n", target_ui_type);

            // create suil host
            suil_host = suil_host_new(
                suil_port_write_func,
                suil_port_index_func,
                suil_port_subscribe_func,
                suil_port_unsubscribe_func
            );

            // create suil instance
            const LilvNode* plugin_uri_n = lilv_plugin_get_uri(plugin_ctl->lv2_plugin_data);
            const LilvNode* ui_uri_n = lilv_ui_get_uri(target_ui_n);
            const LilvNode* ui_bundle_n = lilv_ui_get_bundle_uri(target_ui_n);
            const LilvNode* ui_binary_n = lilv_ui_get_binary_uri(target_ui_n);

            const char* ui_bundle_path = lilv_node_get_path(ui_bundle_n, NULL);
            const char* ui_binary_path = lilv_node_get_path(ui_binary_n, NULL);
            const char* container_type_uri = lilv_node_as_uri(host_type);
            const char* plugin_uri = lilv_node_as_uri(plugin_uri_n);
            const char* ui_uri = lilv_node_as_uri(ui_uri_n);
            const char* plugin_name =
                lilv_node_as_string(lilv_plugin_get_name(plugin_ctl->lv2_plugin_data));

#ifdef UI_X11
            // create parent window
            // TODO: embed plugin window
            Display* xdisplay = glfwGetX11Display();
            

            glfwWindowHint(GLFW_VISIBLE, false);
            glfwWindowHint(GLFW_SCALE_TO_MONITOR, true);
            glfwWindowHint(GLFW_RESIZABLE, false);
            GLFWwindow* parent_window = glfwCreateWindow(100, 100, plugin_name, 0, 0);
            //GLFWwindow* parent_window = glfwGetCurrentContext();
            void* parent_window_handle = 
                (void*) glfwGetX11Window(parent_window);
#endif

            // create features list
            LV2_Feature instance_access = {
                LV2_INSTANCE_ACCESS_URI,
                lilv_instance_get_handle(plugin_ctl->instance)
            };

            LV2_Feature idle_interface_feature = {
                LV2_UI__idleInterface,
                NULL
            };

            LV2_Feature no_user_resize_feature = {
                LV2_UI__noUserResize,
                NULL
            };

            LV2UI_Touch touch;
            touch.handle = nullptr;
            touch.touch = __touch;

            LV2_Feature touch_feature = {
                LV2_UI__touch,
                &touch
            };

            LV2_Feature parent_feature = {
                LV2_UI__parent,
                parent_window_handle
            };

            const LV2_Feature* features[] = {
                &instance_access,
                &idle_interface_feature,
                &no_user_resize_feature,
                &parent_feature,
                nullptr,
                nullptr
            };

            suil_instance = suil_instance_new(
                suil_host,
                (void*) this,
                container_type_uri,
                plugin_uri,
                ui_uri,
                target_ui_type,
                ui_bundle_path,
                ui_binary_path,
                features
            );


            if (suil_instance) {
                LV2UI_Idle_Interface* idle_interface =
                    (LV2UI_Idle_Interface*) suil_instance_extension_data(suil_instance, LV2_UI__idleInterface);
                    
                if (idle_interface) {
                    //idle_interface->idle( suil_instance_get_handle(suil_instance) );
                }
                
                SuilWidget widget = suil_instance_get_widget(suil_instance);
                assert(widget);
                _has_custom_ui = true;
#ifdef UI_X11
                window_handle = parent_window;
                Window child_window = (Window) widget;

                XWindowAttributes attributes;
                XGetWindowAttributes(xdisplay, child_window, &attributes);
                glfwSetWindowSize(parent_window, attributes.width, attributes.height);

                {
                    Window w = (Window) parent_window_handle;
                    XReparentWindow(xdisplay, w, glfwGetX11Window(glfwGetCurrentContext()), 40, 40);

                    XEvent ev;
                    memset(&ev, 0, sizeof(ev));
                    ev.xclient.type = ClientMessage;
                    ev.xclient.window = w;
                    ev.xclient.message_type = XInternAtom( xdisplay, "_XEMBED", False );
                    ev.xclient.format = 32;
                    ev.xclient.data.l[0] = CurrentTime;
                    ev.xclient.data.l[1] = 0;
                    ev.xclient.data.l[2] = 0;
                    ev.xclient.data.l[3] = 0;
                    ev.xclient.data.l[4] = 0;
                    XSendEvent(xdisplay, w, False, NoEventMask, &ev);
                    XSync(xdisplay, False);
                }

                //XCompositeRedirectWindow(xdisplay, child_window, CompositeRedirectAutomatic);
                //pixmap = XCompositeNameWindowPixmap(xdisplay, child_window);

                //glfwSetWindowSize(parent_window, window_width, window_height);
                //XCompositeRedirectWindow(xdisplay, (Window) parent_window_handle, CompositeRedirectAutomatic);
                //XMapWindow(xdisplay, (Window) parent_window_handle);
                //pixmap = XCompositeNameWindowPixmap(xdisplay, (Window) parent_window_handle);

                /*
                view_tex_data = new uint8_t[window_width * window_height * 4];
                
                // generate texture
                glGenTextures(1, &view_texture);
                glBindTexture(GL_TEXTURE_2D, view_texture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#ifdef GL_UNPACK_ROW_LENGTH
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_RGBA,
                    window_width,
                    window_height,
                    0,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    nullptr
                );*/
#elif defined(UI_WINDOWS)
                HWND hWnd = (HWND) widget;
                ShowWindow(hWnd, SW_SHOW);
#endif
            } else {
                dbg("ERROR: could not instantiate ui\n");
            }


        } else {
            dbg("ui not supported\n");
        }

        lilv_uis_free(uis);
    } else {
        dbg("plugin has no uis\n");
    }
}

UIHost::~UIHost()
{

#ifdef UI_X11
    if (window_handle) {
        Display* xdisplay = glfwGetX11Display();
        Window xwindow = glfwGetX11Window((GLFWwindow*) window_handle);
        
        //glDeleteTextures(1, &view_texture);
        delete[] view_tex_data;
    }
#elif defined(UI_WINDOWS)
    // TODO: delete window
#endif

    if (suil_instance) {
        suil_instance_free(suil_instance);
    }

    if (suil_host) {
        suil_host_free(suil_host);
    }
}

void UIHost::show() {
    if (!window_handle) return;

    Display* xdisplay = glfwGetX11Display();
    GLFWwindow* glfw_window = (GLFWwindow*) window_handle;
    XMapWindow(xdisplay, glfwGetX11Window(glfw_window));
}

void UIHost::hide() {
    if (!window_handle) return;
    
    Display* xdisplay = glfwGetX11Display();
    GLFWwindow* glfw_window = (GLFWwindow*) window_handle;
    XUnmapWindow(xdisplay, glfwGetX11Window(glfw_window));
}

void UIHost::render()
{
#ifdef UI_X11
    if (!window_handle) return;

    Display* xdisplay = glfwGetX11Display();
    Window xwindow = glfwGetX11Window( (GLFWwindow*) window_handle );
    XWindowAttributes attributes;
    XGetWindowAttributes(xdisplay, xwindow, &attributes);

    /*
    auto prev = std::chrono::high_resolution_clock::now();

    XImage* image = XGetImage(xdisplay, pixmap, 0, 0, attributes.width, attributes.height, AllPlanes, ZPixmap);
    assert(image);
    
    // todo: read directly from image data
    int k = 0;
    for (int i = 0; i < attributes.height; i++) {
        for (int j = 0; j < attributes.width; j++) {
            auto pixel = XGetPixel(image, j, i);
            view_tex_data[k++] = pixel & image->red_mask >> 16;
            view_tex_data[k++] = (pixel & image->green_mask) >> 8;
            view_tex_data[k++] = (pixel & image->blue_mask);
            view_tex_data[k++] = 255;
        }
    }

    XFree(image);

    auto now = std::chrono::high_resolution_clock::now();
    auto t = std::chrono::duration_cast<std::chrono::milliseconds>(now - prev);
    dbg("ms: %li\n", t.count());
    
    glBindTexture(GL_TEXTURE_2D, view_texture);
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        attributes.width,
        attributes.height,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        view_tex_data
    );

    ImGui::Image((void*)(intptr_t)view_texture, ImVec2(attributes.width, attributes.height));*/
#endif
}