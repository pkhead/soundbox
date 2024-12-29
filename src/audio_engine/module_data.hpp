#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "dsp.hpp"
#include "ring_buffer.hpp"

namespace modules {
    typedef unsigned int ModuleID;
    typedef unsigned int ModulatorSourceID;

    enum class ModuleControlDataType : uint8_t {FLOAT, DOUBLE, INT32, INT64, BOOL, UNKNOWN = UINT8_MAX };
    enum class ModulatorOperationType : uint8_t { MULT, ADD, BOOLEAN };
    enum class ModulatorSourceType : uint8_t { LFO, ENVELOPE, UNKNOWN = UINT8_MAX };
    enum class LFOType : uint8_t { SINE, TRIANGLE, SQUARE, SAW, RAMP };

    template <typename T>
    inline static constexpr ModuleControlDataType ctl_data_type() noexcept;

    template <>
    inline constexpr ModuleControlDataType ctl_data_type<float>() noexcept
        { return ModuleControlDataType::FLOAT; }
    template <>
    inline constexpr ModuleControlDataType ctl_data_type<double>() noexcept
        { return ModuleControlDataType::DOUBLE; }
    template <>
    inline constexpr ModuleControlDataType ctl_data_type<int32_t>() noexcept
        { return ModuleControlDataType::INT32; }
    template <>
    inline constexpr ModuleControlDataType ctl_data_type<int64_t>() noexcept
        { return ModuleControlDataType::INT64; }
    template <>
    inline constexpr ModuleControlDataType ctl_data_type<bool>() noexcept
        { return ModuleControlDataType::BOOL; }
    
    /**
    * Holds information about a module class.
    **/
    struct ModuleInfo
    {
        std::string class_name;
        std::string name;
        std::string author;

        int audio_input = 0;
        int audio_output = 0;
        int midi_input = -1;
        int midi_output = -1;
    }; // struct ModuleInfo

    struct ModulatorSourceParams {
        float attack, decay, sustain, release;

        struct {
            LFOType wavetype;
            float freq;
            float amp;
        } lfo;
    };

    class AudioEngine;
    class AudioRenderer;
    class ModuleProcessor;
    class ModuleCreator;

    template <typename T>
    class ControlValue;

    class ModuleData {
    private:
        static constexpr size_t MESSAGE_PORT_CAPACITY = 512;

        struct ModuleAudioPort
        {
            uint8_t channel_count;
            ModuleID connected_module;
            unsigned int connection_port;
            //bool modulator;

            inline ModuleAudioPort() : channel_count(0), connected_module(0), connection_port(0)
            {}

            inline ModuleAudioPort(uint8_t channel_count, ModuleID connected_module, unsigned int connection_port) :
                channel_count(channel_count),
                connected_module(connected_module),
                connection_port(connection_port)
                //modulator(false)
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

        union Variant {
            float f;
            double d;
            int32_t i32;
            int64_t i64;
            bool b;

            template <typename T>
            inline constexpr const T& get() const noexcept;
            template <typename T>
            inline constexpr T& get() noexcept;
            template <typename T>
            inline constexpr void set(const T v) noexcept;
        };
        
        struct ModulatorOperation {
            ModulatorOperationType optype;

            union {
                float factor;
                float threshold;
            };
        };

        struct ModuleControl
        {
            std::string name;
            ModuleControlDataType data_type = ModuleControlDataType::UNKNOWN;
            Variant value;
            bool can_modulate;
            ModulatorOperation modop;
        };

        struct ModulatorSource;

        struct ModulatorInstance {
            const ModulatorSource *src;

            // adsr envelope
            dsp::ADSR::Instance envelope;
            float cur_level;

            union {
                // phase value (for LFO)
                float phase;

                // sample offset (for sidechaining)
                uint32_t sample_offset;
            };

            inline float next_value() { return src->next_sample(*this); }
        };

        class ModulatorSource {
        public:
            inline bool supports_voices() const { return true; };
            virtual ModulatorInstance instantiate() const;
            virtual void get_params(ModulatorSourceParams &params) const;
            virtual void apply_params(const ModulatorSourceParams &params);

            ModulatorSource(unsigned int sample_rate) : sample_rate(sample_rate) {};
            ~ModulatorSource() {}

        protected:
            dsp::ADSR env_params;
            unsigned int sample_rate;

            void next_envelope_sample(ModulatorInstance &inst) const;
            virtual float next_sample(ModulatorInstance &inst) const = 0;

            friend struct ModulatorInstance;
        };

        class LFOModulatorSource : public ModulatorSource {
        public:
            ModulatorInstance instantiate() const override;
            void get_params(ModulatorSourceParams &params) const override;
            void apply_params(const ModulatorSourceParams &params) override;
            LFOModulatorSource(unsigned int sample_rate) : ModulatorSource(sample_rate) {}

        protected:
            LFOType wavetype;
            float freq, amp;

            float next_sample(ModulatorInstance &inst) const override;
        };

        class EnvelopeModulatorSource : public ModulatorSource {
        public:
            EnvelopeModulatorSource(unsigned int sample_rate) : ModulatorSource(sample_rate) {}

        protected:
            float next_sample(ModulatorInstance &inst) const override;
        };

        struct Modulator
        {
            ModulatorSourceID source;
            std::vector<unsigned int> targets;
        };

        struct ModuleInstance
        {
            std::string name;
            std::string class_name;

            // i think having to check the class name everytime is a bit inefficient,
            // so i have this instead.
            bool is_stereo_mixer;
            bool is_message_duplicator;

            std::vector<ModuleAudioPort> input_audio_ports;
            std::vector<ModuleAudioPort> output_audio_ports;

            std::vector<ModuleMessagePort> input_message_ports;
            std::vector<ModuleMessagePort> output_message_ports;

            std::vector<ModuleControl> controls;
            std::vector<Modulator> modulators;

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

        template <typename T>
        static bool _control_get_ref(ModuleControl &control, T** v) {
            if (control.data_type != ctl_data_type<T>()) return false;
            *v = &control.value.get<T>();
            return true;
        }

        friend class AudioEngine;
        friend class AudioRenderer;
        friend class ModuleCreator;
        friend class ModuleProcessor;

        template <typename T>
        friend class ControlValue;
    }; // class ModuleData

    template <>
    inline constexpr const float& ModuleData::Variant::get<float>() const noexcept { return f; }
    template <>
    inline constexpr const double& ModuleData::Variant::get<double>() const noexcept { return d; }
    template <>
    inline constexpr const int32_t& ModuleData::Variant::get<int32_t>() const noexcept { return i32; }
    template <>
    inline constexpr const int64_t& ModuleData::Variant::get<int64_t>() const noexcept { return i64; }
    template <>
    inline constexpr const bool& ModuleData::Variant::get<bool>() const noexcept { return b; }

    template <>
    inline constexpr float& ModuleData::Variant::get<float>() noexcept { return f; }
    template <>
    inline constexpr double& ModuleData::Variant::get<double>() noexcept { return d; }
    template <>
    inline constexpr int32_t& ModuleData::Variant::get<int32_t>() noexcept { return i32; }
    template <>
    inline constexpr int64_t& ModuleData::Variant::get<int64_t>() noexcept { return i64; }
    template <>
    inline constexpr bool& ModuleData::Variant::get<bool>() noexcept { return b; }

    template <>
    inline constexpr void ModuleData::Variant::set<float>(const float v) noexcept { f = v; }
    template <>
    inline constexpr void ModuleData::Variant::set<double>(const double v) noexcept { d = v; }
    template <>
    inline constexpr void ModuleData::Variant::set<int32_t>(const int32_t v) noexcept { i32 = v; }
    template <>
    inline constexpr void ModuleData::Variant::set<int64_t>(const int64_t v) noexcept { i64 = v; }
    template <>
    inline constexpr void ModuleData::Variant::set<bool>(const bool v) noexcept { b = v; }
} // namespace modules