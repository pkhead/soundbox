#pragma once
#include "../plugins.h"

#include <lilv/lilv.h>
#include <lv2/log/log.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>

namespace plugins
{
    class lv2_error : public std::runtime_error
    {
    public:
        lv2_error(const std::string& what = "lv2 error") : std::runtime_error(what) {}
    };

    class Lv2Plugin : public PluginModule
    {
    private:
        std::string plugin_uri;
        LilvInstance* instance;

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

        float song_last_tempo = -1.0f;
        bool song_last_playing = false;

        float* input_combined;

        std::vector<float*> audio_input_bufs;
        std::vector<float*> audio_output_bufs;
        std::vector<ControlInputPort*> ctl_in;
        std::vector<ControlOutputPort*> ctl_out;
        std::vector<Parameter*> parameters;
        bool is_state_init = false;

        Parameter* find_parameter(LV2_URID id) const;

        static constexpr size_t ATOM_SEQUENCE_CAPACITY = 1024;

        struct AtomSequenceBuffer {
            LV2_Atom_Sequence header;
            uint8_t data[ATOM_SEQUENCE_CAPACITY];
        };

        std::vector<AtomSequenceBuffer*> msg_in;
        std::vector<AtomSequenceBuffer*> msg_out;

        AtomSequenceBuffer* midi_in = nullptr;
        AtomSequenceBuffer* midi_out = nullptr;
        AtomSequenceBuffer* time_in = nullptr;
        AtomSequenceBuffer* patch_in = nullptr;
        AtomSequenceBuffer* patch_out = nullptr;

        LV2_Atom_Forge forge;

        LV2_URID_Map map;
        LV2_Feature map_feature;
        LV2_URID_Unmap unmap;
        LV2_Feature unmap_feature;
        LV2_Log_Log log;
        LV2_Feature log_feature;

        const LV2_Feature* features[4] {
            &map_feature,
            &unmap_feature,
            &log_feature,
            NULL
        };

        // list of displayed values
        struct InterfaceDisplay {
            bool is_parameter;
            bool read_only;

            union {
                ControlInputPort* port_in;
                ControlOutputPort* port_out;
                Parameter* param;
            };
        };
        std::vector<InterfaceDisplay> input_displays;
        std::vector<InterfaceDisplay> output_displays;

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
        Lv2Plugin(audiomod::DestinationModule& dest, const PluginData& data);
        ~Lv2Plugin();

        virtual PluginType plugin_type() { return PluginType::Lv2; };

        static std::vector<PluginData> get_data(const char* path);

        void start() override;
        void stop() override;
        void process(
            float** inputs,
            float* output,
            size_t num_inputs,
            size_t buffer_size,
            int sample_rate,
            int channel_count
        ) override;
        void save_state(std::ostream& ostream) const override;
        bool load_state(std::istream& istream, size_t size) override;

        virtual void event(const audiomod::MidiEvent* event) override;
        virtual size_t receive_events(void** handle, audiomod::MidiEvent* buffer, size_t capacity) override;
        virtual void flush_events() override;

        virtual int control_value_count() const override;
        virtual int output_value_count() const override;
        virtual ControlValue get_control_value(int index) override;
        virtual OutputValue get_output_value(int index) override;
        virtual void control_value_change(int index) override;

        static const char* get_standard_paths();
        static void scan_plugins(const std::vector<std::string>& paths, std::vector<PluginData>& data_out);

        static void lilv_init();
        static void lilv_fini();
    }; // class LadspaPlugin
} // namespace plugins