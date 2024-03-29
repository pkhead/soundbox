#pragma once
#undef Bool
#include <lilv/lilv.h>
#include <lv2/log/log.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/worker/worker.h>
#include <lv2/ui/ui.h>
#include <lv2/options/options.h>
#include <suil/suil.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <thread>
#include <unordered_map>
#include <functional>
#include <memory>

#include "../../worker.h"
#include "../../audio.h"
#include "../../plugins.h"
#include "../../winmgr.h"

#ifdef ENABLE_GTK2
#include <gtk/gtk.h>
#endif

#ifdef UI_X11
#include <GL/glx.h>
#include <GL/glext.h>
#include <X11/Xlib.h>
#undef None
#endif

#include <iostream>

#define RDFS_PREFIX "http://www.w3.org/2000/01/rdf-schema#"

using namespace plugins;

namespace lv2 {
    extern LilvWorld* LILV_WORLD;

    void lv2_init(int* argc, char*** argv);
    void lv2_fini();

    // don't want to include Xlib in a header
    // (because it sucks)
    // (why are there classes named Window and Display without a prefix)
    // (why are there macros simply titled "None")
    typedef void* WindowHandle;

    struct LilvNodeUriList {
        LilvNode* a;
        LilvNode* rdfs_label;
        LilvNode* rdfs_range;

        LilvNode* lv2_InputPort;
        LilvNode* lv2_OutputPort;
        LilvNode* lv2_AudioPort;
        LilvNode* lv2_ControlPort;
        LilvNode* lv2_designation;
        LilvNode* lv2_control;
        LilvNode* lv2_connectionOptional;
        LilvNode* lv2_Parameter;
        LilvNode* lv2_symbol;
        LilvNode* lv2_requiredFeature;
        LilvNode* lv2_optionalFeature;

        LilvNode* atom_AtomPort;
        LilvNode* atom_bufferType;
        LilvNode* atom_Double;
        LilvNode* atom_Sequence;
        LilvNode* atom_supports;

        LilvNode* midi_MidiEvent;

        LilvNode* time_Position;

        LilvNode* patch_Message;
        LilvNode* patch_writable;
        LilvNode* patch_readable;

        LilvNode* state_interface;
        LilvNode* state_loadDefaultState;

        LilvNode* units_unit;
        LilvNode* units_db;

        LilvNode* ui_X11UI;
        LilvNode* ui_WindowsUI;
        LilvNode* ui_GtkUI;
        LilvNode* ui_parent;
        LilvNode* ui_plugin;
        LilvNode* ui_portNotification;
        LilvNode* ui_notifyType;
        LilvNode* ui_portIndex;
    } extern URI;

    /**
    * A wrapper around a Lilv pointers that is
    * automatically freed when it goes out of scope
    **/
    template <class T>
    struct Lilv_ptr
    {
        // check that it is a pointer to a Lilv struct
        static_assert(
            std::is_same<LilvNode, T>::value ||
            std::is_same<LilvState, T>::value ||
            std::is_same<LilvNodes, T>::value ||
            std::is_same<LilvUIs, T>::value
        );

    private:
        T* node;
    public:
        constexpr inline Lilv_ptr() : node(nullptr) {};
        constexpr inline Lilv_ptr(T* node) : node(node) {};
        ~Lilv_ptr();

        inline constexpr T* operator->() const noexcept {
            return node;
        }

        inline constexpr operator T*() const noexcept {
            return node;
        }

        inline constexpr operator bool() const noexcept {
            return node;
        }

        inline constexpr T* operator=(T* new_node) noexcept {
            node = new_node;
            return node;
        }
    };

    template<>
    inline Lilv_ptr<LilvNode>::~Lilv_ptr() {
        if (node) lilv_node_free(node);
    }

    template<>
    inline Lilv_ptr<LilvNodes>::~Lilv_ptr() {
        if (node) lilv_nodes_free(node);
    }

    template<>
    inline Lilv_ptr<LilvState>::~Lilv_ptr() {
        if (node) lilv_state_free(node);
    }

    /**
    * Implementation of the URI feature
    * The URI feature maps IDs to URI strings
    **/
    namespace uri {
        extern std::vector<std::string> URI_MAP;

        LV2_URID map(const char* uri);
        const char* unmap(LV2_URID urid);

        // lv2 urid feature
        LV2_URID map_callback(LV2_URID_Map_Handle handle, const char* uri);
        const char* unmap_callback(LV2_URID_Map_Handle handle, LV2_URID urid);
    }

    /**
    * Implementation of the log feature.
    * It should hopefully be obvious what the logging feature does.
    **/
    namespace log {
        int printf(LV2_Log_Handle handle, LV2_URID type, const char* fmt, ...);
        int vprintf(LV2_Log_Handle handle, LV2_URID type, const char* fmt, va_list ap);
    }

    /**
    * Implementation of the worker feature.
    * The worker feature allows execution of code outside of the audio processing context.
    * (in a different thread)
    **/
    class WorkerHost
    {
    private:
        // a payload will contain a pointer to this class at the start
        static constexpr size_t USERDATA_CAPACITY = WorkScheduler::DATA_CAPACITY - sizeof(WorkerHost*);
        static constexpr size_t RESPONSE_QUEUE_CAPACITY = WorkScheduler::QUEUE_CAPACITY * 4;
        
        struct work_data_payload_t
        {
            WorkerHost* self;
            uint8_t userdata[USERDATA_CAPACITY];
        };

        struct WorkerResponse {
            size_t size;
            uint8_t data[USERDATA_CAPACITY];
        };
        
        MessageQueue response_queue;
    
    public:
        WorkerHost(WorkScheduler& work_scheduler);

        WorkScheduler& work_scheduler;
        LV2_Worker_Interface* worker_interface;
        LV2_Handle instance;

        static LV2_Worker_Status _schedule_work(
            LV2_Worker_Schedule_Handle handle,
            uint32_t size, const void* data
        );

        static void _work_proc(void* data, size_t size);
        static LV2_Worker_Status _worker_respond(
            LV2_Worker_Respond_Handle handle,
            uint32_t size,
            const void* data
        );

        void process_responses();
        void end_run();
    };

    enum class UnitType
    {
        None,
        dB
    };

    /**
    * This is a control editable by the user
    **/
    struct ControlInputPort
    {
        std::string name;
        std::string symbol;
        int port_index;
        const LilvPort* port_handle;
        float value;

        UnitType unit;

        float min, max;
        float default_value;
    };

    /**
    * This is used for analysis probably.
    **/
    struct ControlOutputPort
    {
        std::string name;
        std::string symbol;
        int port_index;
        const LilvPort* port_handle;
        float value;
    };

    /**
    * "Used to assign meaning to controls"
    * for example, EnvelopeControls, FilterControls, CompressorControls, e.t.c.
    **/
    class Parameter
    {
    public:
        LV2_URID id;
        std::string label;
        bool is_writable;
        bool is_readable;
        bool did_change; // set from control_value_changed
        
        enum Type {
            TypeInt,
            TypeLong,
            TypeFloat,
            TypeDouble,
            TypeBool,
            TypePath,
            TypeString
        } type;

        union value {
            int _int;
            long _long;
            float _float;
            double _double;
            bool _bool;
        } v;

        std::string str;

        Parameter(const char* urid, const char* label, const char* type);
        size_t size() const;
        bool set(const void* value, uint32_t expected_size, uint32_t expected_type);
        const void* get(uint32_t* size) const;

        const char* type_uri() const;
    };

    struct InterfaceDisplay {
        bool is_parameter;
        bool read_only;

        union {
            ControlInputPort* port_in;
            ControlOutputPort* port_out;
            Parameter* param;
        };
    };

    static constexpr size_t ATOM_SEQUENCE_CAPACITY = 1024;

    /**
    * Buffer for an Atom sequence. Atom sequences are the primary
    * method of communicating messages to and from plugins.
    **/
    struct AtomSequenceBuffer {
        LV2_Atom_Sequence header;
        uint8_t data[ATOM_SEQUENCE_CAPACITY];
    };

    struct AtomSequencePort {
        const LilvPort* port_handle;
        const std::string symbol;

        AtomSequenceBuffer data;

        // this MessageQueue is used to pass data
        // from the UI thread to the processing thread
        MessageQueue shared_queue;
    };

    /**
    * Struct used to send control port notifications to the
    * plugin's UI
    **/
    struct ControlPortNotification
    {
        uint32_t port_index;
        uint32_t buffer_size;
        uint32_t format;
        const void* buffer;
    };

    struct PortData {
        enum Type {
            Control,
            AtomSequence
        } type;
        bool is_output;

        // this is owned by the underlying ControlInputPort or ControlOutputPort
        // using a char* easier than a string& here
        const char* symbol;

        union {
            ControlInputPort* ctl_in;
            ControlOutputPort* ctl_out;
            AtomSequencePort* sequence;
        };
    };

    /**
    * This is the LV2 plugin host... obviously
    * But this class only handles the audio side of plugin hosting.
    * UI hosting is done in another class.
    **/
    class Lv2PluginHost
    {
    private:
        audiomod::ModuleContext& _modctx;

        float song_last_tempo = -1.0f;
        bool song_last_playing = false;

        float* input_combined;

        std::vector<float*> audio_input_bufs;
        std::vector<float*> audio_output_bufs;

        std::vector<Parameter*> parameters;
        bool is_state_init = false;

        LV2_Atom_Forge forge;

        LV2_URID_Map map;
        LV2_Feature map_feature;
        LV2_URID_Unmap unmap;
        LV2_Feature unmap_feature;
        LV2_Log_Log log;
        LV2_Feature log_feature;
        LV2_Worker_Schedule lv2_worker_schedule;
        LV2_Feature work_schedule_feature;
        LV2_Options_Option options[1];
        LV2_Feature options_feature;

        const LV2_Feature* features[6] {
            &map_feature,
            &unmap_feature,
            &log_feature,
            &work_schedule_feature,
            &options_feature,
            NULL
        };

        lv2::WorkerHost worker_host;

        void _set_port_value(const char* port_symbol, const void* value, uint32_t size, uint32_t type);
        const void* _get_port_value(const char* port_symbol, uint32_t* size, uint32_t type);

        std::atomic<bool> _writing_notifs = false;
        
        static void set_port_value_callback(
            const char* port_symbol,
            void* user_data,
            const void* value,
            uint32_t size,
            uint32_t type
        );

        static const void* get_port_value_callback(
            const char* port_symbol,
            void* user_data,
            uint32_t* size,
            uint32_t type
        );

    public:
        Lv2PluginHost(audiomod::ModuleContext& modctx, const PluginData& data, WorkScheduler& scheduler);
        ~Lv2PluginHost();
        void destroy();

        Song* song;

        std::unordered_map<uint32_t, PortData> ports;
        
        std::vector<ControlInputPort*> ctl_in;
        std::vector<ControlOutputPort*> ctl_out;
        std::vector<AtomSequencePort*> msg_in;
        std::vector<AtomSequencePort*> msg_out;
        AtomSequencePort* midi_in = nullptr;
        AtomSequencePort* midi_out = nullptr;
        AtomSequencePort* time_in = nullptr;
        AtomSequencePort* patch_in = nullptr;
        AtomSequencePort* patch_out = nullptr;

        std::string plugin_uri;
        LilvInstance* instance;
        const PluginData& plugin_data;
        const LilvPlugin* lv2_plugin_data;

        // list of displayed values        
        std::vector<InterfaceDisplay> input_displays;
        std::vector<InterfaceDisplay> output_displays;

        Parameter* find_parameter(LV2_URID id) const;
        
        std::unordered_map<int, std::shared_ptr<MessageQueue>> port_notification_targets;
        void port_subscribe(uint32_t port_index, std::shared_ptr<MessageQueue>& queue);
        void port_unsubscribe(uint32_t port_index);
        
        // function to get shared atom sequence buffer
        //SharedData<AtomSequenceBuffer>*
        //    get_shared_sequence_buffer(uint32_t port_index);

        void start();
        void stop();
        void process(
            float** inputs,
            float* output,
            size_t num_inputs,
            size_t buffer_size,
            int sample_rate,
            int channel_count
        );
        void event(const audiomod::MidiEvent& event);
        void queue_event(const audiomod::MidiEvent& event);
        void send_events(audiomod::ModuleBase& target);
        void save_state(std::ostream& ostream);
        bool load_state(std::istream& istream, size_t size);

        inline bool is_writing_notifications() const { return _writing_notifs; }
    }; // class Lv2PluginHost

    /**
    * This is the host for the UI feature.
    **/
    class UIHost
    {
    private:
        Lv2PluginHost* plugin_ctl = nullptr;
        SuilHost* suil_host = nullptr;
        SuilInstance* suil_instance = nullptr;
        bool _has_custom_ui = false;
        LV2UI_Idle_Interface* idle_interface;

        bool use_gtk = false;

#ifdef ENABLE_GTK2
        union {
            GLFWwindow* ui_window = nullptr;
            GtkWidget* gtk_window;
        };
        bool gtk_close_window = false;
#else
        GLFWwindow* ui_window = nullptr;
#endif
        // libsuil instance callbacks
        static uint32_t suil_port_index_func(
            SuilController controller,
            const char* port_symbol
        );

        static uint32_t suil_port_subscribe_func(
            SuilController controller,
            uint32_t port_index,
            uint32_t protocol,
            const LV2_Feature* const* features
        );

        static uint32_t suil_port_unsubscribe_func(
            SuilController controller,
            uint32_t port_index,
            uint32_t protocol,
            const LV2_Feature* const* features
        );

        static void suil_port_write_func(
            SuilController controller,
            uint32_t port_index,
            uint32_t buffer_size,
            uint32_t protocol,
            void const* buffer
        );

        void suil_touch_func(
            SuilController controller,
            uint32_t port_index,
            bool grabbed
        );

        static void __touch(LV2UI_Feature_Handle handle, uint32_t port_index, bool grabbed);

        // this is called by the audio processing thread
        static void update_port(
            UIHost& self,
            uint32_t port_index,
            PortData::Type type,
            const void* data
        );

        // these are copies of the data located in the ports of the plugin host
        // these are stored to deliever port notifications to the client
        // plugin in a thread-safe manner
        struct ctl_port_data_t {
            float previous;
            float value;
        };

        std::unordered_map<int, ctl_port_data_t> ctl_port_data;
        std::unordered_map<int, std::shared_ptr<MessageQueue>> port_queues;

        // features
        LV2_URID_Map map = {nullptr, uri::map_callback};
        LV2_Feature map_feature = {LV2_URID__map, &map};

        LV2_URID_Unmap unmap = {nullptr, uri::unmap_callback};
        LV2_Feature unmap_feature = {LV2_URID__unmap, &unmap};

        LV2_Log_Log log = {nullptr, log::printf, log::vprintf};
        LV2_Feature log_feature = {LV2_LOG__log, &log};

        LV2UI_Touch touch = {nullptr, UIHost::__touch};
        LV2_Feature touch_feature = {LV2_UI__touch, &touch};

        LV2_Feature idle_interface_feature = {LV2_UI__idleInterface, NULL};
        LV2_Feature no_user_resize_feature = {LV2_UI__noUserResize, NULL};

        LV2_Worker_Schedule lv2_worker_schedule;
        LV2_Feature work_schedule_feature;
        LV2_Feature instance_access;
        LV2_Feature parent_feature;

        // dynamically allocated because of parent feature
        std::vector<LV2_Feature*> features;

        // use x11 composite to get opengl texture
#ifdef COMPOSITING
        std::unique_ptr<WindowTexture> window_texture;
#endif

        bool _is_embedded = false;
        int _buttons = 0; // number of mouse buttons down on the window

        WindowManager& window_manager;
        
    public:
        UIHost(Lv2PluginHost* host, WindowManager& window_manager);
        ~UIHost();

        void destroy();
        void show();
        void hide();
        bool render();
        inline bool is_embedded() const { return _is_embedded; };

        // at startup, check compatibility with GL extension
        // required for embedding windows
        static void check_compatibility();

        inline bool has_custom_ui() const {
            return _has_custom_ui;
        };
    }; // class UIHost

#ifdef ENABLE_GTK2
    void gtk_process();
#endif
} // namespace lv2