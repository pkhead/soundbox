#pragma once
#include <vector>
#include <soundio.h>

class AudioDevice {
private:
    SoundIoDevice* device;
    SoundIoOutStream* outstream;
    SoundIoRingBuffer* ring_buffer;
    size_t buf_capacity;

public:
    AudioDevice(const AudioDevice&) = delete;
    AudioDevice(SoundIo* soundio, int output_device);
    ~AudioDevice();

    int sample_rate() const;
    int num_channels() const;

    bool queue(const float* buffer, const size_t buffer_size);
    int num_queued_frames() const;

    void write(SoundIoOutStream* stream, int frame_count_min, int frame_count_max);
};

namespace audiomod {
    enum NoteEventKind {
        NoteOn,
        NoteOff
    };

    struct NoteOnEvent {
        int key;
        float freq;
        float volume;
    };

    struct NoteOffEvent {
        int key;
    };

    struct NoteEvent {
        NoteEventKind kind;
        union {
            NoteOnEvent note_on;
            NoteOffEvent note_off;
        };
    };

    class ModuleBase;

    class ModuleOutputTarget {
    protected:
        std::vector<ModuleBase*> _inputs;

    public:
        ModuleOutputTarget(const ModuleOutputTarget&) = delete;
        ModuleOutputTarget();
        ~ModuleOutputTarget();

        // returns true if the removal was not successful (i.e., the specified module isn't an input)
        bool remove_input(ModuleBase* module);
        void add_input(ModuleBase* module);
    };

    class ModuleBase : public ModuleOutputTarget {
    protected:
        ModuleOutputTarget* _output;
        bool _has_interface;
        
        float* _audio_buffer;
        size_t _audio_buffer_size;

        virtual void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) = 0;
    public:
        bool show_interface;

        ModuleBase(const ModuleBase&) = delete;
        ModuleBase(bool has_interface);
        ~ModuleBase();

        virtual void event(const NoteEvent& event) {};
        void connect(ModuleOutputTarget* dest);
        void disconnect();
        void remove_all_connections();

        inline bool has_interface() const; // does this module have an interface?
        virtual bool render_interface() { return false; }; // render the ImGui interface

        float* get_audio(size_t buffer_size, int sample_rate, int channel_count);
    };

    class DestinationModule : public ModuleOutputTarget {
    private:
        float* _audio_buffer;
        size_t _prev_buffer_size;
    public:
        DestinationModule(const DestinationModule&) = delete;
        DestinationModule(AudioDevice& device, size_t buffer_size);
        ~DestinationModule();

        AudioDevice& device;
        double time;
        size_t buffer_size;

        size_t process(float** output);
    };

    class WaveformSynth : public ModuleBase {
    protected:
        struct Voice {
            int key;
            float freq;
            float volume;
            double phase[3];
            double time;
            double release_time;
            float release_env;
        };

        std::vector<Voice> voices;
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
    public:
        WaveformSynth();

        enum WaveformType {
            Sine = 0,
            Square = 1,
            Sawtooth = 2,
            Triangle = 3
        } waveform_types[3];
        float volume[3];
        float panning[3];
        int coarse[3];
        float fine[3];

        float attack = 0.0f;
        float decay = 0.0f;
        float sustain = 1.0f;
        float release = 0.0f;

        void event(const NoteEvent& event) override;
        bool render_interface() override;
    };

    class VolumeModule : public ModuleBase {
    protected:
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        float cur_volume[2];
        float last_sample[2];
    
    public:
        VolumeModule();
        float volume = 1.0f;
        float panning = 0.0f;
    };
}