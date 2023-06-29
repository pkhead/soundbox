#pragma once
#include <lilv/lilv.h>
#include <lv2/log/log.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/worker/worker.h>
#include <lv2/ui/ui.h>
#include <suil/suil.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <thread>
#include "../../worker.h"
#include "../../audio.h"
#include "../../plugins.h"

#ifdef ENABLE_GTK2
#include <gtk/gtk.h>
#endif

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
    } extern URI;

    /**
    * A wrapper around a LilvNode* that is
    * automatically freed when it goes out of scope
    **/
    struct LilvNode_ptr
    {
    private:
        LilvNode* node;
    public:
        LilvNode_ptr() : node(nullptr) {};
        LilvNode_ptr(LilvNode* node) : node(node) {};
        ~LilvNode_ptr() {
            if (node)
                lilv_node_free(node);
        }

        inline LilvNode* operator->() const noexcept {
            return node;
        }

        inline operator LilvNode*() const noexcept {
            return node;
        }

        inline operator bool() const noexcept {
            return node;
        }

        inline LilvNode* operator=(LilvNode* new_node) noexcept {
            node = new_node;
            return node;
        }
    };

    namespace uri {
        extern std::vector<std::string> URI_MAP;

        LV2_URID map(const char* uri);
        const char* unmap(LV2_URID urid);

        // lv2 urid feature
        LV2_URID map_callback(LV2_URID_Map_Handle handle, const char* uri);
        const char* unmap_callback(LV2_URID_Map_Handle handle, LV2_URID urid);
    }

    // logging feature
    namespace log {
        int printf(LV2_Log_Handle handle, LV2_URID type, const char* fmt, ...);
        int vprintf(LV2_Log_Handle handle, LV2_URID type, const char* fmt, va_list ap);
    }

    // worker feature
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
            std::atomic<bool> active = false;
            size_t size;
            uint8_t data[USERDATA_CAPACITY];
        } worker_responses[RESPONSE_QUEUE_CAPACITY];

    
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

    struct ControlOutputPort
    {
        std::string name;
        std::string symbol;
        int port_index;
        const LilvPort* port_handle;
        float value;
    };

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

        struct AtomSequenceBuffer {
            LV2_Atom_Sequence header;
            uint8_t data[ATOM_SEQUENCE_CAPACITY];
        };

    struct PortData {
        enum Type {
            Control,
            AtomSequence
        } type;
        bool is_output;

        union {
            ControlInputPort* ctl_in;
            ControlOutputPort* ctl_out;
            AtomSequenceBuffer* sequence;
        };
    };

    class Lv2PluginHost
    {
    private:
        audiomod::DestinationModule& _dest;

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

        const LV2_Feature* features[5] {
            &map_feature,
            &unmap_feature,
            &log_feature,
            &work_schedule_feature,
            NULL
        };

        lv2::WorkerHost worker_host;

        void _set_port_value(const char* port_symbol, const void* value, uint32_t size, uint32_t type);
        const void* _get_port_value(const char* port_symbol, uint32_t* size, uint32_t type);
        
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
        Lv2PluginHost(audiomod::DestinationModule& dest, const PluginData& data, WorkScheduler& scheduler);
        ~Lv2PluginHost();

        Song* song;

        std::vector<ControlInputPort*> ctl_in;
        std::vector<ControlOutputPort*> ctl_out;

        std::vector<AtomSequenceBuffer*> msg_in;
        std::vector<AtomSequenceBuffer*> msg_out;

        AtomSequenceBuffer* midi_in = nullptr;
        AtomSequenceBuffer* midi_out = nullptr;
        AtomSequenceBuffer* time_in = nullptr;
        AtomSequenceBuffer* patch_in = nullptr;
        AtomSequenceBuffer* patch_out = nullptr;
        
        std::vector<PortData> ports;

        std::string plugin_uri;
        LilvInstance* instance;
        const PluginData& plugin_data;
        const LilvPlugin* lv2_plugin_data;

        // list of displayed values        
        std::vector<InterfaceDisplay> input_displays;
        std::vector<InterfaceDisplay> output_displays;

        Parameter* find_parameter(LV2_URID id) const;

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
        void event(const audiomod::MidiEvent* event);
        size_t receive_events(void** handle, audiomod::MidiEvent* buffer, size_t capacity);
        void flush_events();
        void save_state(std::ostream& ostream) const;
        bool load_state(std::istream& istream, size_t size);

        void imgui_interface();
    }; // class Lv2PluginController

    // ui feature
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
        
    public:
        UIHost(Lv2PluginHost* host);
        ~UIHost();

        void show();
        void hide();
        bool render();

        inline bool has_custom_ui() const {
            return _has_custom_ui;
        };
    };

#ifdef ENABLE_GTK2
    void gtk_process();
#endif
} // namespace lv2