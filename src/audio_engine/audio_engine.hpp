#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#include <unordered_map>
#include <thread>
#include <portaudio.h>
#include "audio_engine/module_data.hpp"
#include "audio_renderer.hpp"

namespace modules
{
    // TODO: the amount of forward declarations i make is quite stupid.
    class ModuleHost;
    class ModuleCreator;
    class ModuleProcessor;
    class AudioRenderer;

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
        static constexpr size_t MESSAGE_PORT_CAPACITY = 512;

        struct MessageHeader
        {
            unsigned int size;
        };

        struct ModuleDestroyQueueItem
        {
            ModuleID id;
            std::shared_ptr<ModuleData::ModuleInstance> module;
        };

        struct ModulatorSourceDestroyQueueItem
        {
            ModulatorSourceID id;
            std::shared_ptr<ModuleData::ModulatorSource> source;
        };

        struct ModulatorSourceData
        {
            ModulatorSourceType type;
            ModulatorSourceParams params;
            std::shared_ptr<ModuleData::ModulatorSource> source;
        };

        static ModuleID _next_module_id;
        static ModulatorSourceID _next_modsrc_id;

        std::unordered_map<ModuleID, std::shared_ptr<ModuleData::ModuleInstance>> _modules;
        std::unordered_map<ModulatorSourceID, ModulatorSourceData> _modu_srcs;

        std::vector<ModuleDestroyQueueItem> _destroy_queue;
        std::vector<ModulatorSourceDestroyQueueItem> _modusrc_destroy_queue;

        std::atomic_bool _is_engine_runnning;
        std::unique_ptr<AudioRenderer> renderer;
        bool _is_graph_dirty;
        bool _need_resend_modsrcs;
        std::vector<ModuleID> _dirty_modules; // modulators are dirty

        std::unordered_map<std::string, std::unique_ptr<ModuleHost>> _hosts;
        std::vector<ModuleInfo> _available_module_classes;

        PaStream* _pa_stream;
        unsigned int _sample_rate;
        unsigned int _output_channels;
        const size_t _frames_per_buffer;
        std::atomic_uint64_t _frame_time;

        static int _pa_stream_callback(
            const void* input_buffer,
            void* output_buffer,
            unsigned long frame_count,
            const PaStreamCallbackTimeInfo* time_info,
            PaStreamCallbackFlags status_flags,
            void* userdata
        );
        
        static std::vector<unsigned int>::iterator modulator_get_control(ModuleData::Modulator &mod, unsigned int ctl);
        void invalidate_module_modulators(ModuleID mod);

        void _thread_process();
        std::atomic<double> _process_time;
        double _cpu_load;
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

        /**
        * A special module class that has one message input and one message output.
        * Multiple modules can connect to the output port.
        * When a message is sent to this module, it sends the same message to all
        * connected outputs.
        **/
        static const char* MODULE_CLASS_MESSAGE_DUPLICATOR;

        inline unsigned int sample_rate() const {
            return _sample_rate;
        }

        inline size_t frames_per_buffer() const {
            return _frames_per_buffer;
        }

        inline uint64_t frame_time() const { return renderer->frame_time; }
        inline double process_time() const { return _process_time; }
        inline double cpu_load() const { return _cpu_load; };

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

        const ModuleInfo *get_module_info(const std::string &class_name);

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
        bool control_get_index(ModuleID mod_id, const std::string &name, unsigned int &index) const;

        /// Get the value of a module's control.
        template <typename T>
        T control_get_value(ModuleID mod_id, unsigned int index) const
        {
            CHECK_CONTROL_TYPE(T);

            const auto &it = _modules.find(mod_id);
            if (it == _modules.end()) return 0;
            const ModuleData::ModuleInstance &mod = *it->second;
            if (index >= mod.controls.size()) return 0;
            
            T* ptr;
            if (!ModuleData::_control_get_ref<T>((ModuleData::ModuleControl&) mod.controls[index], &ptr)) return 0;
            return *ptr;
        }

        /// Set the value of a module's control.
        template <typename T>
        bool control_set_value(ModuleID mod_id, unsigned int index, const T value)
        {
            CHECK_CONTROL_TYPE(T);

            const auto &it = _modules.find(mod_id);
            if (it == _modules.end()) return false;
            ModuleData::ModuleInstance &mod = *it->second;
            if (index >= mod.controls.size()) return false;

            T* ptr;
            if (!ModuleData::_control_get_ref<T>(mod.controls[index], &ptr)) return false;
            *ptr = value;

            return true;
        }

        ModulatorSourceID create_modsrc(ModulatorSourceType srctype);
        void destroy_modsrc(ModulatorSourceID modsrc_id);

        ModulatorSourceType get_modsrc_type(ModulatorSourceID modsrc_id) const;
        bool get_modsrc_params(ModulatorSourceID modsrc_id, ModulatorSourceParams &params) const;
        bool set_modsrc_params(ModulatorSourceID modsrc_id, const ModulatorSourceParams &params);

        /// Create a modulator for a given module.
        bool create_modulator(ModuleID mod_id, unsigned int &out_mod_index);

        /// Destroy a modulator.
        /// @param mod_id The ID of the module.
        /// @param mod_index The index of the modulator to destroy.
        void destroy_modulator(ModuleID mod_id, unsigned int mod_index);

        unsigned int modulator_count(ModuleID mod_id) const;

        bool modulator_set_source(ModuleID mod_id, unsigned int modu_idx, ModulatorSourceID modsrc_id);
        ModulatorSourceID modulator_get_source(ModuleID mod_id, unsigned int modu_idx) const;

        /// Connect a module's audio output to a modulator.
        /// @param mod_id The ID of the module with the modulator.
        /// @param control_module The ID of the module that will control the modulator.
        /// @param out_port The index of the audio output port from the control module that will control the modulator.
        /// @param mod_index The index of the modulator.
        /// @returns True on success, false on failure.
        //bool connect_modulator(ModuleID mod_id, ModuleID control_module, unsigned int out_port, unsigned int mod_index);

        /// Disconnect a modulator from its control module.
        /// @param mod_id The ID of the module with the modulator.
        /// @param mod_index The index of the modulator.
        /// @returns True on success, false on failure.
        //bool disconnect_modulator_input(ModuleID mod_id, unsigned int mod_index);

        /// Set the modulator to target a control.
        /// @param mod_id The ID of the module with the modulator.
        /// @param modu The index of the modulator to use.
        /// @param ctl The control port to modulate.
        bool modulator_target(ModuleID mod_id, unsigned int modu_idx, unsigned int ctl);

        /// @returns True if the control was previously targeted, false if not or if there was an error.
        bool modulator_untarget(ModuleID mod_id, unsigned int modu, unsigned int ctl);

        bool modulator_get_targets(ModuleID mod_id, unsigned int modu_idx, std::vector<unsigned int> &out_targets) const;

        bool control_set_mod_op(ModuleID mod_id, unsigned int index, ModulatorOperationType optype, float factor);
        bool control_set_mod_boolop(ModuleID mod_id, unsigned int index, float threshold);

        void update();

        friend class ModuleCreator;
        friend class ModuleProcessor;
        friend class AudioRenderer;
    }; // class AudioEngine

    /// Helper class given to the module host for the instantiation of modules.
    class ModuleCreator
    {
    private:
        ModuleData::ModuleInstance& instance;
        ModuleCreator(ModuleID id, AudioEngine& engine, std::string class_name, ModuleData::ModuleInstance& instance);

        template <typename T>
        ModuleData::ModuleControl _create_module_control(const std::string &name, const T default_value) {
            ModuleData::ModuleControl ctl;
            ctl.name = name;
            ctl.data_type = ctl_data_type<T>();
            ctl.value.set(default_value);
            ctl.modop.optype = ModulatorOperationType::ADD;
            ctl.modop.factor = 0.0f;
            return ctl;
        }

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