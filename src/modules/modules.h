#pragma once
#include <atomic>
#include "../audio.h"

namespace audiomod {
    class WaveformSynth : public ModuleBase {
    protected:
        struct Voice {
            bool active;
            int key;
            float freq;
            float volume;
            double phase[3];
            double time;
            double release_time;
            float release_env;
        };

        static constexpr size_t MAX_VOICES = 16;
        Voice voices[MAX_VOICES];
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        void _interface_proc() override;
    public:
        WaveformSynth(Song* song);

        enum WaveformType: uint8_t {
            Sine = (uint8_t)0,
            Square = (uint8_t)1,
            Sawtooth = (uint8_t)2,
            Triangle = (uint8_t)3
        };
        
        WaveformType waveform_types[3];
        float volume[3];
        float panning[3];
        int coarse[3];
        float fine[3];

        float attack = 0.0f;
        float decay = 0.0f;
        float sustain = 1.0f;
        float release = 0.0f;

        void event(const NoteEvent& event) override;
        size_t save_state(void** output) const override;
        bool load_state(void* state, size_t size) override;
    };

    class VolumeModule : public ModuleBase {
    protected:
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        float cur_volume[2];
        float last_sample[2];
    
    public:
        float volume = 0.5f;
        float panning = 0.0f;
        
        bool mute = false;

        // used for soloing
        bool mute_override = false;

        VolumeModule(Song* song);
        size_t save_state(void** output) const override;
        bool load_state(void* state, size_t size) override;
    };

    class GainModule : public ModuleBase {
    protected:
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        void _interface_proc() override;
        
    public:
        float gain = 0.0f;

        size_t save_state(void** output) const override;
        bool load_state(void* state, size_t size) override;

        GainModule(Song* song);
    };

    class AnalyzerModule : public ModuleBase {
    protected:
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        void _interface_proc() override;

        // 0 = view samples
        // 1 = view spectrogram
        int mode = 0;
        float range = 1.0f;

        size_t buf_size = 0;

        // use double buffering because of concurrency
        std::atomic<int> front_buf = 0;
        std::atomic<bool> ready = false;
        float* left_channel[2] = { nullptr, nullptr };
        float* right_channel[2] = { nullptr, nullptr };

    public:
        AnalyzerModule(Song* song);
        ~AnalyzerModule();
    };
}