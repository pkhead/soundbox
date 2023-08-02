#pragma once
#include <atomic>
#include "../audio.h"
#include "../util.h"

namespace audiomod
{
    class CompressorModule : public ModuleBase {
    protected:
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        void _interface_proc() override;
        
        // keep two copies of the module state, one for the
        // processing thread, and another for the ui thread.
        struct module_state {
            float input_gain,
                  output_gain,
                  threshold,
                  ratio,
                  decay,
                  attack;
        } process_state, ui_state;

        struct analytics_t {
            float in_volume[2];
            float out_volume[2];
        } process_analytics, ui_analytics;

        struct message_t {
            enum Type : uint8_t {
                ModuleState, ReceiveAnalytics, RequestAnalytics
            } type;

            union {
                module_state mod_state;
                analytics_t analytics;
            };
        };

        MessageQueue process_queue;
        MessageQueue ui_queue;
        bool waiting = false; // waiting for analytics response

        // these are used only by the processing thread       
        float _limit[2];

        static constexpr size_t BUFFER_SIZE = 1024;
        float _in_buf[2][BUFFER_SIZE];
        float _out_buf[2][BUFFER_SIZE];
        int _buf_i[2];
    public:
        void save_state(std::ostream& ostream) override;
        bool load_state(std::istream&, size_t size) override;

        CompressorModule(DestinationModule& dest);
    };
}