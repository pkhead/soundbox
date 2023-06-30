#include "lv2interface.h"
#include "lv2internal.h"
#include "../../util.h"
#include <lv2/ui/ui.h>
#include <suil/suil.h>
#include <lv2/instance-access/instance-access.h>
#include <cstdlib>
#include <imgui.h>
#include <chrono>
#include <functional>

#ifdef ENABLE_GTK2
// gtk 2.0 support
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#endif

#ifdef UI_X11
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/XTest.h>
#elif defined(UI_WINDOWS)
#include <windows.h>
#endif

#ifdef UI_X11
#define GLFW_EXPOSE_NATIVE_X11
#elif defined(UI_WINDOWS)
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#definde GLFW_EXPOSE_NATIVE_GLX
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <GL/glx.h>
#include <GL/glext.h>

using namespace lv2;

uint32_t UIHost::suil_port_index_func(SuilController controller, const char* port_symbol)
{
    UIHost* self = (UIHost*) controller;

    for (const auto& [ index, port_data ] : self->plugin_ctl->ports)
    {
        if (strcmp(port_data.symbol, port_symbol) == 0)
            return index;
    }

    dbg("WARNING: could not find index of port %s\n", port_symbol);
    return LV2UI_INVALID_PORT_INDEX;
}

uint32_t UIHost::suil_port_subscribe_func(
    SuilController controller,
    uint32_t port_index,
    uint32_t protocol,
    const LV2_Feature* const* features
) {
    dbg("called suil_port_subscribe_func\n");
    return 1;
}

uint32_t UIHost::suil_port_unsubscribe_func(
    SuilController controller,
    uint32_t port_index,
    uint32_t protocol,
    const LV2_Feature* const* features
) {
    dbg("called suil_port_unsubscribe_func\n");
    return 1;
}

void UIHost::suil_port_write_func(
    SuilController controller,
    uint32_t port_index,
    uint32_t data_size,
    uint32_t protocol,
    void const* data
) {
    UIHost* self = (UIHost*) controller;
    auto it = self->plugin_ctl->ports.find(port_index);

    // if port does not exist
    if (it == self->plugin_ctl->ports.end())
        return;
    
    const auto& [ index, port ] = *it;
    if (port.is_output) return;

    // input float
    if (protocol == 0) {
        if (port.type == PortData::Control) {
            if (data_size == sizeof(float))
                port.ctl_in->value = *((float*) data);
            else
                dbg("WARNING: UI attempted to write port with protocol 0, but data_size != sizeof(float)\n");
        }
    }

    else if (protocol == uri::map(LV2_ATOM__eventTransfer))
    {
        if (port.type == PortData::AtomSequence) {
            // TODO: this is not thread-safe
            if (data_size < 128) {
                struct {
                    int64_t time;
                    uint8_t data[128];
                } payload;

                payload.time = 0;
                memcpy(&payload.data, data, data_size);

                lv2_atom_sequence_append_event(
                    &port.sequence->data.header,
                    ATOM_SEQUENCE_CAPACITY,
                    (LV2_Atom_Event*) &payload
                );
            } else {
                dbg("WARNING: Needed %i bytes for event transfer, but can only hold 128\n", data_size);
            }
        }
    }

    // unknown type
    else {
        const char* str = uri::unmap(protocol);
        dbg("WARNING: unknown protocol %s\n", str ? str : "(null)");
    }
}

void UIHost::suil_touch_func(
    SuilController controller,
    uint32_t port_index,
    bool grabbed
) {
    dbg("called suil_touch_func");
    abort();
}

void UIHost::__touch(LV2UI_Feature_Handle handle, uint32_t port_index, bool grabbed) {
    dbg("touch\n");
}

UIHost::UIHost(Lv2PluginHost* __plugin_controller, WindowManager& _win_manager) : window_manager(_win_manager)
{
    const LilvNode* host_type =
    #ifdef UI_WINDOWS
        URI.ui_WindowsUI;
    #else
        URI.ui_X11UI;
    #endif

    const LilvNode* gtk_host_type = URI.ui_GtkUI;

    plugin_ctl = __plugin_controller;

    Lilv_ptr uis = lilv_plugin_get_uis(plugin_ctl->lv2_plugin_data);
    
    if (uis)
    {
        const LilvUI* target_ui_n = nullptr;
        const char* target_ui_type = nullptr;
        unsigned min_support = 999999; // some arbitrarily high number

        // find the ui widget that is most supported by suil
        LILV_FOREACH (uis, i, uis)
        {
            const LilvUI* ui_n = lilv_uis_get(uis, i);

            const LilvNode* x11_ui_type_n;
            const LilvNode* gtk_ui_type_n;
            const LilvNode* ui_type_n;
            unsigned support;

            bool is_gtk = false;

            unsigned x11_support =
                lilv_ui_is_supported(ui_n, suil_ui_supported, host_type, &x11_ui_type_n);

#ifdef ENABLE_GTK2
            unsigned gtk_support =
                lilv_ui_is_supported(ui_n, suil_ui_supported, gtk_host_type, &gtk_ui_type_n);
#else
            // make sure gtk is never chosen
            unsigned gtk_support = 99999999;
#endif
            if (x11_support != 0 && x11_support <= gtk_support) {
                support = x11_support;
                ui_type_n = x11_ui_type_n;
                is_gtk = false;
            } else {
                support = gtk_support;
                ui_type_n = gtk_ui_type_n;
                is_gtk = true;
            }

            if (support == 0) continue;
            if (support < min_support) {
                min_support = support;
                target_ui_n = ui_n;
                target_ui_type = lilv_node_as_uri(ui_type_n);
                use_gtk = is_gtk;

                if (support == 1) break;
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
            const char* plugin_uri = lilv_node_as_uri(plugin_uri_n);
            const char* ui_uri = lilv_node_as_uri(ui_uri_n);
            const char* plugin_name =
                lilv_node_as_string(lilv_plugin_get_name(plugin_ctl->lv2_plugin_data));

            GLFWwindow* parent_window = nullptr;
            void* parent_window_handle = nullptr;

            lv2_worker_schedule = {this, WorkerHost::_schedule_work};
            work_schedule_feature = {LV2_WORKER__schedule, &lv2_worker_schedule};
            instance_access = {LV2_INSTANCE_ACCESS_URI, lilv_instance_get_handle(plugin_ctl->instance)};

            // check if it has the parent feature
            bool needs_parent =
                lilv_world_ask(LILV_WORLD, ui_uri_n, URI.lv2_requiredFeature, URI.ui_parent) ||
                lilv_world_ask(LILV_WORLD, ui_uri_n, URI.lv2_optionalFeature, URI.ui_parent);
            
            if (use_gtk)
            {
#ifdef ENABLE_GTK2
                const char* container_type_uri = lilv_node_as_uri(gtk_host_type);

                features = {
                    &map_feature,
                    &unmap_feature,
                    &log_feature,
                    &work_schedule_feature,
                    &idle_interface_feature,
                    &no_user_resize_feature,
                    &instance_access,
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
                    features.data()
                );

                if (suil_instance) {
                    idle_interface =
                        (LV2UI_Idle_Interface*) suil_instance_extension_data(suil_instance, LV2_UI__idleInterface);
                    
                    _has_custom_ui = true;

                    gboolean (*close_callback)(GtkWidget* widget, GdkEvent* event, gpointer data) =
                        [](GtkWidget* widget, GdkEvent* event, gpointer data) -> gboolean
                    {
                        UIHost* self = (UIHost*) data;
                        self->gtk_close_window = true;
                        return true;
                    };

                    GtkWidget* widget = (GtkWidget*) suil_instance_get_widget(suil_instance);
                    assert(widget);

                    gtk_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
                    gtk_container_border_width(GTK_CONTAINER(gtk_window), 0);
                    gtk_container_add(GTK_CONTAINER(gtk_window), widget);
                    gtk_window_set_title(GTK_WINDOW(gtk_window), plugin_name);
                    g_signal_connect(GTK_OBJECT(gtk_window), "delete-event", G_CALLBACK(close_callback), (gpointer)this);

                    dbg("successfully instantiate custom plugin UI\n");
                } else {
                    dbg("ERROR: could not instantiate ui\n");
                }
#else
                dbg("ERROR: Attempt to load unsupported GTK2 UI");
#endif
            }
            else
            {
                const char* container_type_uri = lilv_node_as_uri(host_type);

                // create native parent window
                glfwWindowHint(GLFW_VISIBLE, false);
                glfwWindowHint(GLFW_SCALE_TO_MONITOR, true);
                glfwWindowHint(GLFW_RESIZABLE, false);
                
                // this will be resized to the child window later
                parent_window = glfwCreateWindow(100, 100, plugin_name, 0, 0);
    #ifdef UI_X11
                parent_window_handle = (void*) glfwGetX11Window(parent_window);
                // make child window float above parent window
                /*XSetTransientForHint(
                    glfwGetX11Display(),
                    (Window) parent_window_handle,
                    glfwGetX11Window( glfwGetCurrentContext() )
                );*/
    #elif defined(UI_WINDOWS)
                parent_window_handle = (void*) glfwGetWin32Window(parent_window);
    #endif
                
                // create features list
                parent_feature = {
                    LV2_UI__parent,
                    (void*)parent_window_handle
                };

                features = {
                    &map_feature,
                    &unmap_feature,
                    &log_feature,
                    &work_schedule_feature,
                    &idle_interface_feature,
                    &no_user_resize_feature,
                    &instance_access,
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
                    features.data()
                );

                if (suil_instance) {
                    idle_interface =
                        (LV2UI_Idle_Interface*) suil_instance_extension_data(suil_instance, LV2_UI__idleInterface);
                    
                    ui_window = parent_window;
                    _has_custom_ui = true;

                    SuilWidget child_window = suil_instance_get_widget(suil_instance);

                    // set size of parent to child window
                    int window_width, window_height;
                    
                    {
#ifdef UI_X11
                        XWindowAttributes attrib;
                        XGetWindowAttributes(glfwGetX11Display(), (Window) child_window, &attrib);
                        window_width = attrib.width;
                        window_height = attrib.height;
                        
                        XReparentWindow(
                            glfwGetX11Display(),
                            (Window) parent_window_handle,
                            glfwGetX11Window(window_manager.root_window()),
                            0, 0
                        );
#elif def(UI_WINDOWS)
                        abort(); // TODO
#endif
                    }

                    glfwSetWindowSize(parent_window, window_width, window_height);

                    _is_embedded = window_manager.can_composite();
                    dbg("successfully instantiate custom plugin UI\n");
                } else {
                    dbg("ERROR: could not instantiate ui\n");
                }
            }

            // if instantiation was successful,
            // read portNotifications
            if (suil_instance)
            {
                Lilv_ptr port_notifications = lilv_world_find_nodes(
                    LILV_WORLD,
                    ui_uri_n,
                    URI.ui_portNotification,
                    nullptr
                );

                if (port_notifications) {
                    LILV_FOREACH (nodes, iter, port_notifications) {
                        const LilvNode* notif_data = lilv_nodes_get(port_notifications, iter);
                        
                        // plugin to monitor
                        Lilv_ptr plugin_n =
                            lilv_world_get(LILV_WORLD, notif_data, URI.ui_plugin, nullptr);

                        // make sure it is the current plugin
                        if (!(plugin_n && lilv_node_equals(plugin_n, plugin_uri_n)))
                            continue;
                        
                        // symbol to monitor
                        Lilv_ptr symbol_n =
                            lilv_world_get(LILV_WORLD, notif_data, URI.lv2_symbol, nullptr);

                        // or, index
                        Lilv_ptr index_n =
                            lilv_world_get(LILV_WORLD, notif_data, URI.ui_portIndex, nullptr);

                        // notify type
                        Lilv_ptr notify_type_n =
                            lilv_world_get(LILV_WORLD, notif_data, URI.ui_notifyType, nullptr);

                        if (symbol_n || index_n) {
                            // get port index
                            int index;

                            if (index_n) {
                                index = lilv_node_as_int(index_n);

                            // index not specified, get index from port symbol instead
                            } else {
                                index = suil_port_index_func(this, lilv_node_as_string(symbol_n));
                                if (index == LV2UI_INVALID_PORT_INDEX) 
                                    continue;   
                            }

                            auto port_it = plugin_ctl->ports.find(index);

                            if (port_it != plugin_ctl->ports.end()) {
                                plugin_ctl->ports.find(index);
                                const PortData& port_data = port_it->second; 

                                dbg("subscribe to port %i\n", index);

                                // the valid union member will be determined
                                // by the port type 
                                PortNotificationTarget data;

                                if (port_data.type == PortData::Control) {
                                    auto ptr = std::make_unique<ctl_port_data_t>();
                                    
                                    // initialize port data
                                    ptr->value = port_data.is_output ?
                                        port_data.ctl_out->value :
                                        port_data.ctl_in->value;

                                    ptr->previous = ptr->value;
                                    
                                    // TODO: whats the point of using unique_ptr then
                                    data.control = &ptr->value;
                                    ctl_port_data[index] = std::move(ptr);
                                } else if (port_data.type == PortData::AtomSequence) {
                                    auto raw_ptr = new SharedData<AtomSequenceBuffer>({
                                        { sizeof(LV2_Atom_Sequence_Body), uri::map(LV2_ATOM__Sequence) },
                                        { 0, 0 }
                                    });

                                    std::unique_ptr ptr =
                                        std::unique_ptr<SharedData<AtomSequenceBuffer>>(raw_ptr);
                                    
                                    data.atom_sequence = raw_ptr;
                                    seq_port_data[index] = std::move(ptr);
                                } else {
                                    throw std::runtime_error("Invalid PortData type");
                                }

                                plugin_ctl->port_subscribe(index, data);
                            } else {
                                dbg("WARNING: plugin request to subscribe to invalid port %i\n", index);
                            }
                        } else {
                            dbg("WARNING: port notification connection but portIndex or symbol is not specified\n");
                        }
                    }
                }
            }
        } else {
            dbg("ui not supported\n");
        }
    } else {
        dbg("plugin has no uis\n");
    }
}

UIHost::~UIHost()
{
    if (suil_instance) {
        dbg("free LV2 UI host\n");
        suil_instance_free(suil_instance);
    }

    if (suil_host) {
        suil_host_free(suil_host);
    }

#ifdef ENABLE_GTK2
    if (use_gtk) {
        gtk_widget_destroy(gtk_window);
    } else
#endif
    if (ui_window) {
#ifdef UI_X11
        window_texture = nullptr;
#endif
        glfwDestroyWindow(ui_window);
    }
}

void UIHost::show() {
    if (!ui_window) return;

#ifdef ENABLE_GTK2
    if (use_gtk) {
        gtk_widget_show_all(gtk_window);

        // set transfient for parent glfw window
        Window wid = (Window) glfwGetX11Window(glfwGetCurrentContext());
        Window child = gdk_x11_drawable_get_xid(gtk_widget_get_window(GTK_WIDGET(gtk_window)));
        XSetTransientForHint(glfwGetX11Display(), child, wid);
        return;
    }
#endif
    glfwShowWindow(ui_window);

#ifdef UI_X11
    if (window_manager.can_composite()) {
        window_texture = std::make_unique<WindowTexture>(ui_window);
        if (!window_texture->is_loaded()) window_texture = nullptr;
        XLowerWindow(glfwGetX11Display(), glfwGetX11Window(ui_window));
    }
#endif
}

void UIHost::hide() {
    if (!ui_window) return;

#ifdef ENABLE_GTK2
    if (use_gtk) {
        gtk_widget_hide(gtk_window);
        return;
    }
#endif

#ifdef UI_X11
    window_texture = nullptr;
#endif
    glfwHideWindow(ui_window);
}

bool UIHost::render()
{
    // TODO: is embedding possible
    if (!use_gtk && glfwWindowShouldClose(ui_window)) {
        glfwSetWindowShouldClose(ui_window, false);
        return false;
    }

#ifdef ENABLE_GTK2
    if (gtk_close_window) {
        gtk_close_window = false;
        return false;
    }
#endif

    // receive port notifications
    if (!plugin_ctl->is_writing_notifications())
    {
        // controls
        for (auto& [index, data] : ctl_port_data)
        {
            if (data->value != data->previous) {
                data->previous = data->value;

                suil_instance_port_event(
                    suil_instance,
                    index,
                    sizeof(float),
                    0,
                    &data->value
                );
            }
        }

        // atom sequences
        for (auto& [index, sequence] : seq_port_data)
        {
            // get events and clear buffer
            AtomSequenceBuffer events; {
                auto handle = sequence->get_handle();
                events = handle.get(); // copy
                lv2_atom_sequence_clear(&handle.get().header);
            };

            if (events.header.atom.size > sizeof(LV2_Atom_Sequence_Body))
            {
                dbg("received data: %i\n", events.header.atom.size);

                int event_transfer_urid = uri::map(LV2_ATOM__eventTransfer);

                LV2_ATOM_SEQUENCE_FOREACH (&events.header, ev)
                {
                    suil_instance_port_event(
                        suil_instance,
                        index,
                        ev->body.size + sizeof(ev->body),
                        event_transfer_urid,
                        &ev->body
                    );
                }
            }
        }
    }

    if (idle_interface)
    {
        LV2_Handle handle = suil_instance_get_handle(suil_instance);
        assert(handle);
        if (idle_interface->idle(handle)) {
            return false;
        }
    }

#ifdef UI_X11
    if (window_texture)
    {
        Display* xdisplay = glfwGetX11Display();
        Window wid = glfwGetX11Window(ui_window);

        int win_width, win_height;
        glfwGetWindowSize(ui_window, &win_width, &win_height);

        ImVec2 cursor = ImGui::GetCursorPos();
        ImVec2 cursor_screen = ImGui::GetCursorScreenPos();
        ImVec2 mouse_pos = ImGui::GetMousePos();

        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::SetCursorPos(ImVec2(0.0f, ImGui::GetFrameHeight()));
        ImGui::Image((void*)(size_t)window_texture->texture_id(), ImVec2(win_width, win_height));
        
        // if window is hovered over, the plugin window is active
        // can't use InvisibleButton and ImGui::IsItemHovered(), because
        // it only returns true if the root window is in focus
        if (window_manager.is_window_hovered()) {
            dbg("%i\n", glfwGetMouseButton(ui_window, GLFW_MOUSE_BUTTON_LEFT));
            XMoveWindow(xdisplay, wid, cursor_screen.x, cursor_screen.y);
        }

        // not hovered, move it off-screen
        else
            XMoveWindow(xdisplay, wid, -win_width, -win_height);
        
        // get mouse position on window
        /*ImVec2 mouse_pos = ImGui::GetMousePos();
        static int last_mouse_x = -1;
        static int last_mouse_y = -1;

        int mouse_x = mouse_pos.x - cursor_screen.x;
        int mouse_y = mouse_pos.y - cursor_screen.y;

        last_mouse_x = mouse_x;
        last_mouse_y = mouse_y;*/
    }
#endif

    return true;
}

#ifdef ENABLE_GTK2
void lv2::gtk_process()
{
    while (gtk_events_pending())
        gtk_main_iteration();
}
#endif