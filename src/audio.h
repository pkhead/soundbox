#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <atomic>
#include <stddef.h>
#include <stdint.h>
#include <vector>
#include <portaudio.h>
#include "worker.h"
#include "util.h"

class AudioDevice {
private:
    PaStream* pa_stream;

    // a ring buffer that should hold up to 0.5 seconds of audio
    // size in bytes: sample_rate() / 2 * sizeof(float)
    RingBuffer<float> ring_buffer;
    
    size_t _thread_buffer_size = 0;
    size_t _thread_buf_pos = 0;
    float* _thread_audio_buffer = nullptr; // this is not owned by the AudioDevice

    //std::atomic<double> _time = 0.0;
    std::atomic<uint64_t> _frames_written = 0;

    static constexpr float SAMPLE_RATE = 48000;

    // this function will convert userdata to an AudioDevice* and call _pa_stream_callback
    static int _pa_stream_callback_raw(
        const void* input_buffer,
        void* output_buffer,
        unsigned long frame_count,
        const PaStreamCallbackTimeInfo* time_info,
        PaStreamCallbackFlags status_flags,
        void* userdata
    );

    int _pa_stream_callback(
        const void* input_buffer,
        void* output_buffer,
        unsigned long frame_count,
        const PaStreamCallbackTimeInfo* time_info,
        PaStreamCallbackFlags status_flags
    );
public:
    static bool _pa_start();
    static void _pa_stop();

    AudioDevice(const AudioDevice&) = delete;
    AudioDevice(int output_device);
    ~AudioDevice();

    inline int sample_rate() const { return SAMPLE_RATE; };
    inline int num_channels() const { return 2; };
    inline double time() const { return Pa_GetStreamTime(pa_stream); };
    inline uint64_t frames_written() const { return _frames_written; }
    void stop();

    void queue(float* buf, size_t size);
    size_t samples_queued() const;
};

class Song;

namespace plugins
{
    class PluginManager;
}

namespace audiomod {
    enum NoteEventKind {
        NoteOn,
        NoteOff
    };

    struct MidiMessage {
        uint8_t status;

        union {
            struct {
                uint8_t key;
                uint8_t velocity;
            } note;
        };

        size_t size() const;
    };

    struct MidiEvent {
        uint64_t time;
        MidiMessage msg;
    };

    struct NoteEvent {
        NoteEventKind kind;
        int key;
        float volume;

        size_t write_midi(MidiMessage* out) const;
        bool read_midi(const MidiMessage* in);
    };

    class ModuleBase;
    class DestinationModule;

    class ModuleOutputTarget {
    protected:
        std::vector<ModuleBase*> _inputs;
        std::vector<float*> _input_arrays;

    public:
        ModuleOutputTarget(const ModuleOutputTarget&) = delete;
        ModuleOutputTarget();
        ~ModuleOutputTarget();

        // returns true if the removal was not successful (i.e., the specified module isn't an input)
        bool remove_input(ModuleBase* module);
        void add_input(ModuleBase* module);
        inline const std::vector<ModuleBase*>& get_inputs() const { return _inputs; }
    };

    class ModuleBase : public ModuleOutputTarget {
    protected:
        ModuleOutputTarget* _output;
        DestinationModule& _dest;
        bool _has_interface;
        bool _is_released = false;
        bool _interface_shown = false;
        
        float* _audio_buffer;
        size_t _audio_buffer_size;

        virtual void _interface_proc() {};
    public:
        const char* id;
        std::string name;
        Song* song = nullptr;
        const char* parent_name = nullptr; // i keep forgetting to explicity set values to nullptr!

        ModuleBase(const ModuleBase&) = delete;
        ModuleBase(DestinationModule& dest, bool has_interface);
        virtual ~ModuleBase();

        // mark module for deletion
        // modules can't be deleted immediately because multithreading
        // so only delete it when thread is done
        void release();
        
        // free all modules that were marked for deletion
        static void free_garbage_modules();

        // send a midi event to the module
        virtual void event(const MidiEvent& event) {};

        /**
        * Receive MIDI events from a module.
        *
        * This will dequeue an internal MIDI sequence buffer and write it to the output.
        * If the given buffer is not big enough, it will return. Processing
        * will resume once the function is called again. If there are no MIDI messages
        * left, it should return 0. This should be called in a loop.
        *
        * @param handle A handle for the iterator. When the process begin,
        * this should point to nullptr
        *
        * @returns The number of MIDI messages written to the output
        **/
        virtual size_t receive_events(void** handle, MidiEvent* buffer, size_t capacity) { return 0; };
        
        // Tell the module to clear its event queue after
        // processing is finished
        virtual void flush_events() {};

        // connect this module's output to a target's input
        void connect(ModuleOutputTarget* dest);

        // disconnect this from its output
        void disconnect();

        // disconnect inputs and outputs
        void remove_all_connections();

        inline ModuleOutputTarget* get_output() const { return _output; };

        // does this module have an interface?
        bool has_interface() const;
        
        // render the ImGui interface
        virtual bool render_interface();

        virtual bool show_interface();
        virtual void hide_interface();
        bool interface_shown() const { return _interface_shown; };

        // write the serialized state of the module to a stream
        virtual void save_state(std::ostream& ostream) { return; };

        // load a serialized state. return true if successful, otherwise return false
        virtual bool load_state(std::istream& istream, size_t size) { return true; };

        float* get_audio();
        virtual void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) = 0;
        void prepare_audio(DestinationModule* destination);
    };

    class DestinationModule : public ModuleOutputTarget {
    private:
        float* _audio_buffer;
        size_t _prev_buffer_size;

        uint64_t _frame_time;

        // buffer filled with zeroes if audio is not yet ready
        static constexpr size_t DUMMY_BUFFER_SAMPLE_COUNT = 256;
        float dummy_buffer[DUMMY_BUFFER_SAMPLE_COUNT];
        
        /**
        * Ensure thread safety by having a copy of the audio graph.
        * This copy will be used by the audio thread. When the audio
        * graph is changed, construct a representation of the graph
        * using ModuleNodes and send it to the audio thread. The
        * audio thread will then use this representation for processing,
        * then send the old pointer back to the main thread so that
        * it can be freed.
        * 
        * The main thread wouldn't actually be the main thread, because
        * that may have Vsync -- rather, an auxillary thread that also
        * does the sending of NoteEvents. And also, they wouldn't
        * actually "send messages"; it would just be the changing
        * of flags. I just say that to enforce the concept of ownership.
        */
        struct ModuleNode
        {
            ModuleBase* module = nullptr; // module is nullptr if root
            float* output;
            size_t buf_size;

            ModuleNode** inputs;
            float** input_arrays;
            size_t num_inputs;
        };

        ModuleNode* _thread_audio_graph = nullptr; // the graph the audio thread is using

        // this is set to the new graph if audio_graph is outdated
        std::atomic<ModuleNode*> new_graph = nullptr;
        std::atomic<bool> send_new_graph = false;

        // if !nullptr, this is set to the old audio_graph which needs to be freed
        std::atomic<ModuleNode*> old_graph = nullptr;

        // if graph needs to be constructed on the main thread
        bool is_dirty = false;

        // modules that need to be freed on free_graph
        std::vector<ModuleBase*> garbage_modules;

        // construct a graph of ModuleNodes
        ModuleNode* _create_node(ModuleBase* module);
        void process_node(ModuleNode* node);
        ModuleNode* construct_graph();
        void free_graph(ModuleNode* node);
        void _get_all_modules(const ModuleOutputTarget* target, std::vector<ModuleBase*>& vec) const;
    public:
        DestinationModule(const DestinationModule&) = delete;
        DestinationModule(int sample_rate, int num_channels, size_t buffer_size);
        ~DestinationModule();

        // Returns an std::vector of all modules that contributes
        // to the destination
        std::vector<ModuleBase*> get_all_modules() const;

        int sample_rate;
        int channel_count;
        size_t frames_per_buffer;

        inline uint64_t time_in_frames() const { return _frame_time; };
        inline double time_in_seconds() const { return (double)_frame_time / frames_per_buffer; };

        size_t process(float** output);
        void prepare();
        void reset();
        inline void make_dirty() { is_dirty = true; };
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


    class FXBus
    {
    public:
        FXBus(audiomod::DestinationModule& dest);

        EffectsRack rack;

        class ControllerModule : public ModuleBase
        {
        protected:
            void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;

            // used for calculating loudness
            float smp_count = 0;
            float window_size = 1024;
            float smp_accum[2] = { 0.0f, 0.0f };
        public:
            float analysis_volume[2] = {0.0f, 0.0f};
            float gain = 0.0f;
            bool mute = false;
            bool mute_override = false;

            ControllerModule(audiomod::DestinationModule& dest);
        } controller;

        inline const std::vector<ModuleBase*> get_modules() const
        {
            return rack.modules;
        };

        static constexpr size_t name_capacity = 16;
        char name[16];

        bool interface_open = false;
        bool solo = false;

        // index of target bus
        int target_bus = 0;

        inline void insert(ModuleBase* module, size_t position) { rack.insert(module, position); };
        inline void insert(ModuleBase* module) { rack.insert(module); };
        inline ModuleBase* remove(size_t position) { return rack.remove(position); };
        
        // wrap around EffectsRack methods, because
        // EffectsRack's output should always be the controller module
        inline void connect_input(ModuleBase* input) { rack.connect_input(input); };
        inline bool disconnect_input(ModuleBase* input) { return rack.disconnect_input(input); };
        inline void disconnect_all_inputs() { rack.disconnect_all_inputs(); };

        // these implementations aren't one-liners so maybe inlining isn't a good idea
        ModuleOutputTarget* connect_output(ModuleOutputTarget* output);
        ModuleOutputTarget* disconnect_output();
    };
}
