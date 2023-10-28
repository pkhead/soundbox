#pragma once
#include <vector>
#include <memory>
#include <string>
#include <portaudio.h>

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

namespace audiomod
{
    enum NoteEventKind
    {
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

    // The actual module
    class ModuleBase;

    // Handles connections between modules
    class ModuleNode;
    typedef std::shared_ptr<ModuleNode> ModuleNodeRc;
    
    // Handles ModuleNodes
    class ModuleContext;
    class ModuleNode : public std::enable_shared_from_this<ModuleNode>
    {
        friend ModuleContext;
    
    private:
        std::unique_ptr<ModuleBase> _module;
        ModuleContext& modctx;

        std::vector<float*> input_arrays;
        std::vector<ModuleNodeRc> input_nodes;

        ModuleNodeRc output_node;
        float* output_array;

        bool remove_input(ModuleNodeRc& module);
        void add_input(ModuleNodeRc module);

    public:
        ModuleNode(ModuleContext& modctx, std::unique_ptr<ModuleBase>&& module);
        ~ModuleNode();
        
        // Connect this module's output to a node's input
        void connect(ModuleNodeRc& dest);

        // Disconnect this node from its output
        void disconnect();

        // Disconnect all inputs and outputs
        void remove_all_connections();

        template <class T = ModuleBase>
        inline T& module()
        {
            return dynamic_cast<T&>(*_module);
        }
        
        inline const std::vector<ModuleNodeRc>& get_inputs() const {
            return input_nodes;
        }

        inline ModuleNodeRc& get_output() {
            return output_node;
        }
    };

    class ModuleContext
    {
    friend ModuleNode;

    private:
        std::vector<ModuleNodeRc> nodes;
        ModuleNodeRc _dest; // does not have outputs

        float* audio_buffer;

        uint64_t _frame_time;

        // buffer filled with zeroes if audio is not yet ready
        static constexpr size_t DUMMY_BUFFER_SAMPLE_COUNT = 256;
        float dummy_buffer[DUMMY_BUFFER_SAMPLE_COUNT];

        void make_dirty();
        void process_node(ModuleNode& node);
    
    public:
        ModuleContext(const ModuleContext&) = delete;
        ModuleContext(int sample_rate, int num_channels, size_t buffer_size);
        ~ModuleContext();

        const int sample_rate;
        const int num_channels;
        const int frames_per_buffer;

        inline ModuleNodeRc& destination() {
            return _dest;
        }

        template <class T, class... Args>
        ModuleNodeRc create(Args&&... args)
        {
            std::unique_ptr<T> module = std::make_unique<T>(args...);
            ModuleNodeRc node = std::make_shared<ModuleNode>(*this, std::move(module));
            nodes.push_back(node);
            return node;
        }

        inline uint64_t time_in_frames() const { return _frame_time; };
        inline double time_in_seconds() const { return (double)_frame_time / frames_per_buffer; };

        size_t process(float* &buffer);
    };

    class ModuleBase
    {
    protected:
        bool _has_interface;
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
        ModuleBase(bool has_interface);
        virtual ~ModuleBase();

        /**
        * Process a MIDI event, to be called from the audio thread.
        * @param event The event to be processed 
        **/
        virtual void event(const MidiEvent& event) {};

        /**
        * Queue a MIDI event to be processed on the audio thread.
        * @param event The event to queue
        **/
        virtual void queue_event(const MidiEvent& event) {};

        /**
        * Flush the event queue of the module.
        * @param out_module The module to send possible midi outputs to, if not nullptr
        */
        virtual void flush_events() {};

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
    };
}