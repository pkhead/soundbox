#pragma once
#include "lv2internal.h"

#include "../../plugins.h"
#include "../../worker.h"

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
        lv2::Lv2PluginHost host;
        lv2::UIHost ui_host;

    protected:
        void _interface_proc() override;
    
    public:
        Lv2Plugin(audiomod::DestinationModule& dest, const PluginData& data, WorkScheduler& scheduler, WindowManager& winmgr);
        ~Lv2Plugin();

        virtual PluginType plugin_type() { return PluginType::Lv2; };

        bool show_interface() override;
        bool render_interface() override;
        void hide_interface() override;

        virtual void event(const audiomod::MidiEvent& event) override;
        virtual size_t receive_events(void** handle, audiomod::MidiEvent* buffer, size_t capacity) override;
        virtual void flush_events() override;

        virtual int control_value_count() const override;
        virtual int output_value_count() const override;

        virtual void set_control_value(int index, float value) override;
        virtual ControlValue get_control_value(int index) override;
        virtual OutputValue get_output_value(int index) override;

        virtual void control_value_change(int index) override;

        static const char* get_standard_paths();
        static void scan_plugins(const std::vector<std::string>& paths, std::vector<PluginData>& data_out);

        void process(
            float** inputs,
            float* output,
            size_t num_inputs,
            size_t buffer_size,
            int sample_rate,
            int channel_count
        ) override;
    }; // class Lv2Plugin
} // namespace plugins