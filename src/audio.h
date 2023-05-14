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
    enum class NoteEventKind {
        NoteOn,
        NoteOff
    };

    struct NoteEvent {
        NoteEventKind kind;
        union {
            struct {
                int key;
                float volume;
            } note_on;

            struct {
                int key;
            } note_off;
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
        
        float* _audio_buffer;
        size_t _audio_buffer_size;

        virtual void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) = 0;
    public:
        ModuleBase(const ModuleBase&) = delete;
        ModuleBase();
        ~ModuleBase();

        virtual void event(const NoteEvent& event) {};
        void connect(ModuleOutputTarget* dest);
        void disconnect();
        void remove_all_connections();

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
}