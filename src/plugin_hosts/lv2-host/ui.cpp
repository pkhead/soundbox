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
#include <X11/extensions/Xcomposite.h>
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

            GLFWwindow* parent_window = nullptr;
            void* parent_window_handle = nullptr;

            // create parent window
            glfwWindowHint(GLFW_VISIBLE, false);
            glfwWindowHint(GLFW_SCALE_TO_MONITOR, true);
            glfwWindowHint(GLFW_RESIZABLE, false);
            
            // this will be resized to the child window later
            parent_window = glfwCreateWindow(100, 100, plugin_name, 0, 0);
#ifdef UI_X11
            parent_window_handle = (void*) glfwGetX11Window(parent_window);
#elif defined(UI_WINDOWS)
            parent_window_handle = (void*) glfwGetWin32Window(parent_window);
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
                (void*)parent_window_handle
            };

            const LV2_Feature* features[] = {
                &instance_access,
                &idle_interface_feature,
                &no_user_resize_feature,
                &parent_feature,
                nullptr,
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
                idle_interface =
                    (LV2UI_Idle_Interface*) suil_instance_extension_data(suil_instance, LV2_UI__idleInterface);
                
                ui_window = parent_window;
                _has_custom_ui = true;

                SuilWidget child_window = suil_instance_get_widget(suil_instance);

                // set size of parent to child window
                int window_width, window_height;
#ifdef UI_X11
                {
                    XWindowAttributes attrib;
                    XGetWindowAttributes(glfwGetX11Display(), (Window) child_window, &attrib);
                    window_width = attrib.width;
                    window_height = attrib.height;
                }
#elif def(UI_WINDOWS)
                {
                    abort(); // TODO
                }
#endif
                glfwSetWindowSize(parent_window, window_width, window_height);
                dbg("successfully instantiate custom plugin UI\n");
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
    if (suil_instance) {
        dbg("free UI\n");
        suil_instance_free(suil_instance);
    }

    if (suil_host) {
        suil_host_free(suil_host);
    }

    if (ui_window) {
        glfwDestroyWindow(ui_window);
    }
}

void UIHost::show() {
    if (!ui_window) return;
    glfwShowWindow(ui_window);
}

void UIHost::hide() {
    if (!ui_window) return;
    glfwHideWindow(ui_window);
}

bool UIHost::render()
{
    // TODO: is embedding possible
    if (glfwWindowShouldClose(ui_window)) {
        glfwSetWindowShouldClose(ui_window, false);
        return false;
    }

    if (idle_interface) {
        LV2_Handle handle = suil_instance_get_handle(suil_instance);
        assert(handle);
        if (idle_interface->idle(handle)) {
            return false;
        }
    }

    return true;
}