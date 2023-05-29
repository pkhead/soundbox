#pragma once
#include <string>
#include <stddef.h>
#include <stdint.h>
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
        virtual void _interface_proc() {};
    public:
        bool show_interface;
        const char* id;
        std::string name;

        ModuleBase(const ModuleBase&) = delete;
        ModuleBase(bool has_interface);
        virtual ~ModuleBase();

        // send a note event to the module
        virtual void event(const NoteEvent& event) {};

        // connect this module's output to a target's input
        void connect(ModuleOutputTarget* dest);

        // disconnect this from its output
        void disconnect();

        // disconnect inputs and outputs
        void remove_all_connections();

        inline ModuleOutputTarget* get_output() const;

        // does this module have an interface?
        bool has_interface() const;
        
        // render the ImGui interface
        bool render_interface(const char* channel_name);

        // report a newly allocated block of memory which holds the serialized state of the module
        virtual size_t save_state(void** output) const { return 0; };

        // load a serialized state. return true if successful, otherwise return false
        virtual bool load_state(void* state, size_t size) { return true; };

        float* get_audio(size_t buffer_size, int sample_rate, int channel_count);
    };

    class DestinationModule : public ModuleOutputTarget {
    private:
        float* _audio_buffer;
        size_t _prev_buffer_size;
    public:
        DestinationModule(const DestinationModule&) = delete;
        DestinationModule(int sample_rate, int num_channels, size_t buffer_size);
        ~DestinationModule();

        int sample_rate;
        int channel_count;

        double time;
        size_t buffer_size;

        size_t process(float** output);
    };

    /**
    * A module which holds a doubly-linked list of other modules,
    * grouped together as a single unit.
    **/
    class EffectsRack
    {
    protected:
        std::vector<ModuleBase*> inputs;
        ModuleOutputTarget* output;

    public:
        EffectsRack();
        ~EffectsRack();
        
        std::vector<ModuleBase*> modules;

        void insert(ModuleBase* module, size_t position);
        void insert(ModuleBase* module);
        ModuleBase* remove(size_t position);

        /**
        * Connect a module to the effects rack
        */
        void connect_input(ModuleBase* input);

        /**
        * Connect the effects rack to a module
        * @returns The previous output module
        */
        ModuleOutputTarget* connect_output(ModuleOutputTarget* output);

        /**
        * Disconnect the input from effects rack
        */
        bool disconnect_input(ModuleBase* input);
        void disconnect_all_inputs();

        /**
        * Disconnect the effects rack to its output
        * @returns A pointer to the now disconnected output module
        */
        ModuleOutputTarget* disconnect_output();
    };

    ModuleBase* create_module(const std::string& mod_id);
}