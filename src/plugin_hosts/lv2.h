#pragma once
#include "../plugins.h"

#include <lilv/lilv.h>
#include <lv2/log/log.h>
#include <lv2/atom/atom.h>

namespace plugins
{
    class lv2_error : public std::runtime_error
    {
    public:
        lv2_error(const std::string& what = "lv2 error") : std::runtime_error(what) {}
    };

    class Lv2Plugin : public Plugin
    {
    private:
        LilvInstance* instance;
        audiomod::DestinationModule& dest;

        enum class UnitType
        {
            None,
            dB
        };

        struct ControlInput
        {
            std::string name;
            int port_index;
            const LilvPort* port_handle;
            float value;

            UnitType unit;

            float min, max;
            float default_value;
        };

        struct ControlOutput
        {
            std::string name;
            int port_index;
            const LilvPort* port_handle;
            float value;
        };

        float* input_combined;
        std::vector<float*> audio_input_bufs;
        std::vector<float*> audio_output_bufs;

        std::vector<ControlInput*> ctl_in;
        std::vector<ControlOutput*> ctl_out;

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

        LV2_Atom_Event* midi_read_it; // MIDI out read iterator

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
    
    public:
        Lv2Plugin(audiomod::DestinationModule& dest, const PluginData& data);
        ~Lv2Plugin();

        virtual PluginType plugin_type() { return PluginType::Lv2; };

        static std::vector<PluginData> get_data(const char* path);

        void start() override;
        void stop() override;
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size) override;
        void save_state(std::ostream& ostream) const override;
        bool load_state(std::istream& istream, size_t size) override;

        virtual void event(const audiomod::MidiEvent* event) override;
        virtual size_t receive_events(audiomod::MidiEvent* buffer, size_t capacity) override;

        virtual int control_value_count() const override;
        virtual int output_value_count() const override;
        virtual ControlValue get_control_value(int index) override;
        virtual OutputValue get_output_value(int index) override;

        static const char* get_standard_paths();
        static void scan_plugins(const std::vector<std::string>& paths, std::vector<PluginData>& data_out);

        static void lilv_init();
        static void lilv_fini();
    }; // class LadspaPlugin
} // namespace plugins