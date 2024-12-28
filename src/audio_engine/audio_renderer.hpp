#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <concurrent/readerwriterqueue.h>
#include "module_data.hpp"

#define CHECK_CONTROL_TYPE(T) static_assert( \
        std::is_same<T, float>() || std::is_same<T, double>() || std::is_same<T, std::int32_t>() || std::is_same<T, std::int64_t>() || std::is_same<T, bool>(), \
        "unsupported data type" \
    );

namespace modules
{
    typedef unsigned int ModuleID;

    /**
    * Handles module connections and playback.
    * It runs a different thread where audio processing occurs.
    * Functions here queue changes to it and are sent to the processing thread on update()
    */
    class AudioRenderer
    {
    private:
        struct GraphConnection {
            int index;
            unsigned int from_port;
            unsigned int to_port;
        };

        struct GraphModulationConnection {
            int index;
            unsigned int from_port;
            std::vector<ModuleData::ModulatorControl> targets;
        };

        struct ModuleGraphNode {
            std::shared_ptr<ModuleData::ModuleInstance> module;
            std::vector<ModuleID> dependencies;
            std::vector<ModuleID> dependents;

            std::vector<GraphConnection> audio_inputs;
            std::vector<GraphConnection> audio_outputs;
            std::vector<GraphConnection> message_inputs;
            std::vector<GraphConnection> message_outputs;
            std::vector<GraphModulationConnection> modulators;
        };

        struct ModuleGraph
        {
            std::unordered_map<ModuleID, ModuleGraphNode> nodes;
            std::vector<ModuleID> process_order;
        };

        enum InMessageKind : uint8_t {
            MESSAGE_NEW_GRAPH,
            MESSAGE_UPDATE_MODULATOR_TARGET,
        };

        enum OutMessageKind : uint8_t {
            MESSAGE_GRAPH_UPDATED
        };

        struct InMessage {
            InMessageKind kind;

            union {
                ModuleGraph *graph;
                union {
                    ModuleID mod_id;
                    unsigned int modulator;
                    ModuleData::ModulatorControl params;
                } modulator_target;
            };
        };

        struct OutMessage {
            OutMessageKind kind;
        };
        
        moodycamel::ReaderWriterQueue<InMessage> in_queue;
        moodycamel::ReaderWriterQueue<OutMessage> out_queue;
        ModuleGraph *cur_graph;
        
        void _process_node(ModuleID id);
        void _process_audio_out_node(ModuleProcessor& proc);
        void update_modulator_target(ModuleID mod_id, unsigned int moduidx, const ModuleData::ModulatorControl &params);

        std::atomic<float> process_time;
        std::atomic_uint64_t frame_time;

        AudioEngine &engine;

        float *output_buffer;
        size_t output_buffer_sz;

        inline bool send_message(const InMessage &msg) { return in_queue.try_enqueue(msg); }
        inline bool get_message(OutMessage &msg) { return out_queue.try_dequeue(msg); }

        void render(float *buffer);
        inline size_t buffer_size() const { return output_buffer_sz; }

        static void process_audio_out_node(ModuleProcessor& proc);
        static void process_stereo_mixer_node(ModuleProcessor &proc);
        static void process_message_duplicator_node(ModuleProcessor &proc);

        /// call from main thread (AudioEngine) and send to rendering thread
        /// via send_message
        ModuleGraph* build_graph();

        friend class AudioEngine;
        friend class ModuleCreator;
        friend class ModuleProcessor;
    
    public:
        AudioRenderer(AudioEngine &engine);
        AudioRenderer(const AudioEngine&&) = delete;
        AudioRenderer& operator=(const AudioEngine&&) = delete;
        ~AudioRenderer();
    }; // class AudioEngine

    /// Data passed to the module run function to handle processing I/O
    class ModuleProcessor
    {
    private:
        AudioRenderer::ModuleGraph *const graph;
        AudioRenderer::ModuleGraphNode &node;

        ModuleProcessor(size_t buffer_frame_count, unsigned long frame_time, unsigned int sample_rate, AudioRenderer::ModuleGraph *graph, ModuleID id);

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
            return node.module->controls.size();
        }

        ModuleControlDataType control_type(unsigned int index) const
        {
            if (index >= node.module->controls.size()) return ModuleControlDataType::UNKNOWN;
            return node.module->controls[index].data_type;
        }

        template <typename T>
        T get_control_value(unsigned int index) const
        {
            CHECK_CONTROL_TYPE(T);

            if (index >= node.module->controls.size()) return 0;

            T* ptr;
            if (!ModuleData::_control_get_ref(node.module->controls[index], &ptr)) return 0;
            return *ptr;
        }

        friend class AudioRenderer;
    }; // class ModuleProcessor
} // class modules

#undef CHECK_CONTROL_TYPE