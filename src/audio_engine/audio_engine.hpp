#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <thread>
#include <portaudio.h>

#include "ring_buffer.hpp"
#include "../log.hpp"

namespace modules
{
    typedef unsigned int ModuleID;

    enum class ModuleControlDataType : uint8_t { FLOAT, DOUBLE, INT32, INT64, BOOL, UNKNOWN = UINT8_MAX };

    // TODO: the amount of forward declarations i make is quite stupid.
    class ModuleHost;
    class ModuleCreator;
    class ModuleProcessor;

    /**
    * Holds information about a module class.
    **/
    struct ModuleInfo
    {
        std::string class_name;
        std::string name;
        std::string author;

        bool has_midi_input;
        bool has_audio_input;

        ModuleInfo(const std::string &class_name, const std::string &name, bool has_audio_input = true);
    }; // struct ModuleInfo

    #define CHECK_CONTROL_TYPE(T) static_assert( \
        std::is_same<T, float>() || std::is_same<T, double>() || std::is_same<T, std::int32_t>() || std::is_same<T, std::int64_t>() || std::is_same<T, bool>(), \
        "unsupported data type" \
    );

    /**
    * Handles module connections and playback.
    * It runs a different thread where audio processing occurs.
    * Functions here queue changes to it and are sent to the processing thread on update()
    */
    class AudioEngine
    {
    private:
        struct ModuleAudioPort
        {
            uint8_t channel_count;
            ModuleID connected_module;
            unsigned int connection_port;

            inline ModuleAudioPort() : channel_count(0), connected_module(0), connection_port(0)
            {}

            inline ModuleAudioPort(uint8_t channel_count, ModuleID connected_module, unsigned int connection_port) :
                channel_count(channel_count),
                connected_module(connected_module),
                connection_port(connection_port)
            {}
        };

        struct ModuleMessagePort
        {
            ModuleID connected_module;
            unsigned int connection_port;

            inline ModuleMessagePort() : connected_module(0), connection_port(0)
            {}
            
            inline ModuleMessagePort(ModuleID connected_module, unsigned int connection_port) :
                connected_module(connected_module),
                connection_port(connection_port)
            {}
        };

        struct MessageHeader
        {
            unsigned int size;
        };

        struct ModuleControl
        {
            std::string name;
            ModuleControlDataType data_type;
            union
            {
                float float_value;
                double double_value;
                int32_t int32_value;
                int64_t int64_value;
                bool bool_value;
            };
        };

        struct ModuleInstance
        {
            std::string name;
            std::string class_name;

            // i think having to check the class name everytime is a bit inefficient,
            // so i have this instead.
            bool is_stereo_mixer;

            std::vector<ModuleAudioPort> input_audio_ports;
            std::vector<ModuleAudioPort> output_audio_ports;

            std::vector<ModuleMessagePort> input_message_ports;
            std::vector<ModuleMessagePort> output_message_ports;

            std::vector<ModuleControl> controls;

            void* userdata;
            void (*processor)(ModuleProcessor& processor);
            void (*idle)(AudioEngine &engine, ModuleID id, void *userdata);

            struct
            {
                float* input_dummy_buffer;
                std::vector<float*> output_audio_buffers;
                std::vector<RingBuffer<std::byte>> input_messages;
                std::vector<RingBuffer<std::byte>> output_messages;
            } audio_data;

            ~ModuleInstance()
            {
                delete[] audio_data.input_dummy_buffer;

                for (auto it = audio_data.output_audio_buffers.begin(); it != audio_data.output_audio_buffers.end(); it++)
                    delete[] *it;
            }
        };

        struct ModuleGraphNode;

        struct ModuleGraphConnection
        {
            int from_node_index;
            unsigned int from_port;
            unsigned int to_port;

            inline constexpr ModuleGraphConnection(int from_node_index, unsigned int from_port, unsigned int to_port) :
                from_node_index(from_node_index),
                from_port(from_port),
                to_port(to_port)
            {}
        };

        struct ModuleGraphNode
        {
            std::shared_ptr<ModuleInstance> module;
            std::vector<std::shared_ptr<ModuleGraphNode>> dependencies;

            std::vector<ModuleGraphConnection> audio_inputs;
            std::vector<ModuleGraphConnection> message_inputs;
        };

        struct DestroyQueueItem
        {
            ModuleID id;
            std::shared_ptr<ModuleInstance> module;
        };

        static ModuleID _next_module_id;
        std::unordered_map<ModuleID, std::shared_ptr<ModuleInstance>> _modules;
        std::vector<DestroyQueueItem> _destroy_queue;
        std::mutex _mutex;

        std::thread _thread;
        std::unique_ptr<ModuleGraphNode> _current_graph;
        std::atomic_bool _is_engine_runnning;

        std::unordered_map<std::string, std::unique_ptr<ModuleHost>> _hosts;
        std::vector<ModuleInfo> _available_module_classes;

        PaStream* _pa_stream;
        unsigned int _sample_rate;
        unsigned int _output_channels;
        RingBuffer<float> _audio_ring_buffer;
        std::vector<float> _audio_buffer;
        const size_t _frames_per_buffer;
        bool _is_graph_dirty;
        std::atomic_uint64_t _frame_time;

        static int _pa_stream_callback(
            const void* input_buffer,
            void* output_buffer,
            unsigned long frame_count,
            const PaStreamCallbackTimeInfo* time_info,
            PaStreamCallbackFlags status_flags,
            void* userdata
        );

        //static int _rt_audio_callback(void *output_buffer, void *input_buffer, unsigned int n_buffer_frames, double stream_time, RtAudioStreamStatus status, void *userdata);
        
        void _thread_process();
        void _process_node(ModuleGraphNode& node);

        void _process_audio_out_node(ModuleProcessor& proc);
        static void _s_process_audio_out_node(ModuleProcessor& proc);

        static void _s_process_stereo_mixer_node(ModuleProcessor &proc);

        template <typename T>
        static bool _control_get_ref(ModuleControl &control, T** v);
    public:
        AudioEngine();
        AudioEngine(const AudioEngine&&) = delete;
        AudioEngine& operator=(const AudioEngine&&) = delete;

        ~AudioEngine();

        /**
        * A special module class that serves as the output node for the audio engine.
        **/
        static const char* MODULE_CLASS_AUDIO_OUT;

        /**
        * A special module class that has one stereo audio input and one stereo audio output.
        * Multiple modules can connect their output to the input port.
        * It combines all of the inputs into the singular output signal.
        **/
        static const char* MODULE_CLASS_STEREO_MIXER;

        inline unsigned int sample_rate() const {
            return _sample_rate;
        }

        inline size_t frames_per_buffer() const {
            return _frames_per_buffer;
        }

        inline unsigned long frame_time() const { return _frame_time; }

        /// Register a host.
        /// @returns True if host registration was successful, false if not.
        bool register_host(std::unique_ptr<ModuleHost> &&host);
        
        /// Create a module.
        /// @param mod_class The class name of the module to create.
        /// @returns The ID of the newly created module, or 0 if there was a failure.
        ModuleID create_module(const std::string &mod_class);

        /// Destroy a given module.
        /// @param mod_id The ID of the module to destroy.
        void destroy_module(ModuleID mod_id);

        std::vector<ModuleID> list_modules() const;
        const inline std::vector<ModuleInfo>& available_module_classes() const
        {
            return _available_module_classes;
        }

        /// Check if a module with a given ID exists.
        /// @param mod_id The ID of the module to check
        /// @returns True if the module exists, false if not.
        bool module_exists(ModuleID mod_id) const;

        const std::string& module_class_name(ModuleID mod_id) const;
        const std::string& module_name(ModuleID mod_id) const;

        // module audio i/o
        unsigned int audio_input_count(ModuleID mod_id) const;
        unsigned int audio_output_count(ModuleID mod_id) const;
        uint8_t audio_input_channel_count(ModuleID mod_id, unsigned int index) const;
        uint8_t audio_output_channel_count(ModuleID mod_id, unsigned int index) const;
        void get_audio_input_connection(ModuleID mod_a, unsigned int in_index, ModuleID &mod_b, unsigned int &out_index) const;
        void get_audio_output_connection(ModuleID mod_a, unsigned int out_index, ModuleID &mod_b, unsigned int &in_index) const;
        bool connect_audio(ModuleID mod_a, ModuleID mod_b, unsigned int out_index, unsigned int in_index);
        bool disconnect_audio_output(ModuleID mod, unsigned int out_index);
        bool disconnect_audio_input(ModuleID mod, unsigned int in_index);

        // module message i/o
        unsigned int message_input_count(ModuleID mod_id) const;
        unsigned int message_output_count(ModuleID mod_id) const;
        void get_message_input_connection(ModuleID mod_a, unsigned int in_index, ModuleID &mod_b, unsigned int &out_index) const;
        void get_message_output_connection(ModuleID mod_a, unsigned int out_index, ModuleID &mod_b, unsigned int &in_index) const;
        bool connect_message(ModuleID mod_a, ModuleID mod_b, unsigned int out_index, unsigned int in_index);
        bool disconnect_message_output(ModuleID mod, unsigned int out_index);
        bool disconnect_message_input(ModuleID mod, unsigned int in_index);

        /// Send a message to a given module's message input.
        /// @param mod The ID of the module.
        /// @param index The index of the message input port.
        /// @param data Pointer to the first byte of the data to send.
        /// @param data_size The size of the data to send.
        /// @returns True on success, and false if the given port did not exist or if there was not enough space to send the message. 
        //bool send_message(ModuleID mod, unsigned int index, void* data, unsigned int data_size);

        // module controls
        unsigned int control_count(ModuleID mod_id) const;
        ModuleControlDataType control_data_type(ModuleID mod_id, unsigned int index) const;
        const std::string control_name(ModuleID mod_id, unsigned int index) const;

        /// Get the value of a module's control.
        template <typename T>
        T control_get_value(ModuleID mod_id, unsigned int index) const
        {
            CHECK_CONTROL_TYPE(T);

            const auto &it = _modules.find(mod_id);
            if (it == _modules.end()) return 0;
            const ModuleInstance &mod = *it->second;
            if (index >= mod.controls.size()) return 0;
            
            T* ptr;
            if (!_control_get_ref<T>((ModuleControl&) mod.controls[index], &ptr)) return 0;
            return *ptr;
        }

        /// Set the value of a module's control.
        template <typename T>
        bool control_set_value(ModuleID mod_id, unsigned int index, const T value)
        {
            CHECK_CONTROL_TYPE(T);

            const auto &it = _modules.find(mod_id);
            if (it == _modules.end()) return false;
            ModuleInstance &mod = *it->second;
            if (index >= mod.controls.size()) return false;

            T* ptr;
            if (!_control_get_ref<T>(mod.controls[index], &ptr)) return false;
            *ptr = value;

            return true;
        }

        void update();

        friend class ModuleCreator;
        friend class ModuleProcessor;
    }; // class AudioEngine

    /// Helper class given to the module host for the instantiation of modules.
    class ModuleCreator
    {
    private:
        AudioEngine::ModuleInstance& instance;
        ModuleCreator(ModuleID id, AudioEngine& engine, std::string class_name, AudioEngine::ModuleInstance& instance);

        template <typename T>
        AudioEngine::ModuleControl _create_module_control(const std::string &name, const T default_value);

    public:
        AudioEngine &engine;
        const ModuleID id;
        const std::string class_name;
        std::string &name;
        void* userdata = nullptr;

        /**
        * Process input audio and/or generate audio buffers.
        * Called on a separate audio processing thread.
        **/
        void (*processor)(ModuleProcessor& processor) = nullptr;

        /**
        * Called on the thread that called AudioEngine::update()
        **/
        void (*idle)(AudioEngine &engine, ModuleID id, void *userdata) = nullptr;
        
        void add_audio_input(uint8_t channels);
        void add_audio_output(uint8_t channels);

        void add_message_input();
        void add_message_output();

        template <typename T>
        void add_control(unsigned int index, const std::string &name, const T default_value)
        {
            CHECK_CONTROL_TYPE(T);

            // resize vector to be big enough to have a value at the given index
            if (instance.controls.size() <= index)
                instance.controls.resize(index + 1);

            instance.controls[index] = _create_module_control<T>(name, default_value);
        }

        friend class AudioEngine;
    }; // class ModuleCreator

    /// Data passed to the module run function to handle processing I/O
    class ModuleProcessor
    {
    private:
        AudioEngine::ModuleGraphNode* node;
        ModuleProcessor(size_t buffer_frame_count, unsigned long frame_time, unsigned int sample_rate, AudioEngine::ModuleGraphNode* node);

    public:
        const std::size_t buffer_frame_count;
        const unsigned int sample_rate;
        const unsigned long frame_time;
        const char* const class_name;
        void* const userdata;

        float* audio_input(unsigned int index) const;
        float* audio_output(unsigned int index) const;
        uint8_t audio_input_channels(unsigned int index) const;
        uint8_t audio_output_channels(unsigned int index) const;

        /// Read a singular message from a message port.
        /// @param index The index of the message input port to read from.
        /// @param buffer The destination to copy the message data to.
        /// @param max_length The maximum length of the message, in bytes.
        /// @returns The size of the message, in bytes. If 0, there were no messages to read, or there was a failure.
        unsigned int read_message(unsigned int index, void *buffer, unsigned int max_length);

        /// Send a message to a message port.
        /// @param index The index of the message output port to write to.
        /// @param data Pointer to the first byte of the data to send.
        /// @param data_size The size of the data to send.
        /// @returns True on success, and false if the given port did not exist or if there was not enough space to send the message.
        bool send_message(unsigned int index, void *data, unsigned int data_size);

        unsigned int control_count() const
        {
            return node->module->controls.size();
        }

        ModuleControlDataType control_type(unsigned int index) const
        {
            if (index >= node->module->controls.size()) return ModuleControlDataType::UNKNOWN;
            return node->module->controls[index].data_type;
        }

        template <typename T>
        T get_control_value(unsigned int index) const
        {
            CHECK_CONTROL_TYPE(T);

            if (index >= node->module->controls.size()) return 0;

            T* ptr;
            if (!AudioEngine::_control_get_ref(node->module->controls[index], &ptr)) return 0;
            return *ptr;
        }

        friend class AudioEngine;
    }; // class ModuleProcessor

    /// Abstract class for a module host.
    class ModuleHost
    {
    public:
        ModuleHost() = default;
        ModuleHost(const ModuleHost&&) = delete;
        ModuleHost& operator=(const ModuleHost&&) = delete;
        
        virtual ~ModuleHost()
        {}

        virtual const char* host_id() const = 0;

        /// Initialize the host.
        /// @returns True if successful, false if not.
        virtual bool initialize() = 0;
        
        virtual const std::vector<ModuleInfo> scan_modules() = 0;

        /// Create a module.
        /// @returns True on success, false on failure.
        virtual bool create_module(ModuleCreator& creator) = 0;
        virtual void destroy_module(const std::string class_name, ModuleID id, void *userdata) = 0;
    }; // class ModuleHost
} // class modules

#undef CHECK_CONTROL_TYPE