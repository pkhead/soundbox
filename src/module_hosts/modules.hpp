/**
* Extensions/implementations for the audio engine.
*/
#pragma once
#include <memory>
#include <imguiext/imgui-knobs.h>
#include <ostream>
#include <istream>
#include "../audio_engine/audio_engine.hpp"

namespace modx
{
    /**
    * Handle to a module ID. The module will be destroyed when
    * the handle is destructed.
    */
    class ModuleHandle
    {
    private:
        modules::AudioEngine &_engine;
        modules::ModuleID _id;
    
    public:
        ModuleHandle(modules::AudioEngine &engine, const std::string &class_name);
        ~ModuleHandle();

        inline bool valid() const { return _id != 0; }

        const std::string name() const { return _engine.module_name(_id); }
        std::string class_name() const { return _engine.module_class_name(_id); }
        const modules::ModuleInfo *get_module_info() { return _engine.get_module_info(class_name()); }

        unsigned int audio_input_count() const { return _engine.audio_input_count(_id); };
        unsigned int audio_output_count() const { return _engine.audio_output_count(_id); };
        uint8_t audio_input_channel_count(unsigned int index) const { return _engine.audio_input_channel_count(_id, index); };
        uint8_t audio_output_channel_count(unsigned int index) const { return _engine.audio_output_channel_count(_id, index); };

        bool disconnect_audio_output(unsigned int out_index) { return _engine.disconnect_audio_output(_id, out_index); }
        bool disconnect_audio_input(unsigned int in_index) { return _engine.disconnect_audio_input(_id, in_index); }
        bool connect_audio(ModuleHandle &other_module, unsigned int out_index, unsigned int in_index) { return _engine.connect_audio(_id, other_module._id, out_index, in_index); }

        unsigned int message_input_count() const { return _engine.message_input_count(_id); };
        unsigned int message_output_count() const { return _engine.message_output_count(_id); };

        bool disconnect_message_output(unsigned int out_index) { return _engine.disconnect_message_output(_id, out_index); }
        bool disconnect_message_input(unsigned int in_index) { return _engine.disconnect_message_input(_id, in_index); }
        bool connect_message(ModuleHandle &other_module, unsigned int out_index, unsigned int in_index) { return _engine.connect_message(_id, other_module._id, out_index, in_index); }

        inline unsigned int control_count()
            { return _engine.control_count(_id); }

        inline modules::ModuleControlDataType control_data_type(unsigned int index) const
            { return _engine.control_data_type(_id, index); }

        inline const std::string control_name(unsigned int index) const
            { return _engine.control_name(_id, index); }

        template <typename T>
        T control_get_value(unsigned int index) { return _engine.control_get_value<T>(_id, index); }
        
        template <typename T>
        bool control_set_value(unsigned int index, T value) { return _engine.control_set_value(_id, index, value); }

        inline bool create_modulator(unsigned int &out_mod_index)
            { return _engine.create_modulator(_id, out_mod_index); }
        inline void destroy_modulator(unsigned int mod_index)
            { return _engine.destroy_modulator(_id, mod_index); }
        inline unsigned int modulator_count() const
            { return _engine.modulator_count(_id); }
        inline bool modulator_set_source(unsigned int modu_idx, modules::ModulatorSourceID modsrc_id)
            { return _engine.modulator_set_source(_id, modu_idx, modsrc_id); }
        inline modules::ModulatorSourceID modulator_get_source(unsigned int modu_idx) const
            { return _engine.modulator_get_source(_id, modu_idx); }
        
        inline bool modulator_target(unsigned int modu_idx, unsigned int ctl)
            { return _engine.modulator_target(_id, modu_idx, ctl); }
        inline bool modulator_untarget(unsigned int modu, unsigned int ctl)
            { return _engine.modulator_untarget(_id, modu, ctl); }
        inline bool modulator_get_targets(unsigned int modu_idx, std::vector<unsigned int> &out_targets) const {
            { return _engine.modulator_get_targets(_id, modu_idx, out_targets); }
        }

        inline bool control_set_mod_op(unsigned int index, modules::ModulatorOperationType optype, float factor)
            { return _engine.control_set_mod_op(_id, index, optype, factor); }
        inline bool control_set_mod_boolop(unsigned int index, float threshold)
            { return _engine.control_set_mod_boolop(_id, index, threshold); }
        inline auto control_get_mod_op(unsigned int index, modules::ModulatorOperationType &out_optype, float &out_factor)
            { return _engine.control_get_mod_op(_id, index, out_optype, out_factor); }

        inline modules::AudioEngine& engine() const { return _engine; }
        inline modules::ModuleID id() const { return _id; }
    }; // class ModuleRef

    /**
    * Reference-counted reference to a module handle.
    **/
    typedef std::shared_ptr<ModuleHandle> ModuleRc;

    inline ModuleRc create_module(modules::AudioEngine &engine, const std::string &class_name) {
        return std::make_shared<ModuleHandle>(engine, class_name);
    }

    /**
    * Abstract class for a module implementation.
    **/
    class ModuleBase
    {
    private:
        modules::ModuleID _id;
    
    protected:
        bool ui_knob(
            const char *label,
            unsigned int control_index,
            float v_min,
            float v_max,
            const char *fmt = "%.3f",
            ImGuiKnobFlags flags = 0,
            float speed = 0.0f,
            ImGuiKnobVariant variant = ImGuiKnobVariant_Dot,
            float size = 0,
            int steps = 10
        );

        bool ui_knob_int(
            const char *label,
            unsigned int control_index,
            int v_min,
            int v_max,
            const char *fmt = "%i",
            ImGuiKnobFlags flags = 0,
            float speed = 0.0f,
            ImGuiKnobVariant variant = ImGuiKnobVariant_Dot,
            float size = 0,
            int steps = 10
        );

        template <typename T>
        T get_control_value(unsigned int index) const {
            return engine->control_get_value<T>(id(), index);
        }

        template <typename T>
        bool set_control_value(unsigned int index, T v) const {
            return engine->control_set_value(id(), index, v);
        }
    public:
        ModuleBase(modules::ModuleCreator& creator) : _id(creator.id), engine(&creator.engine) {}
        virtual ~ModuleBase() {}

        modules::AudioEngine *const engine;
        inline modules::ModuleID id() const { return _id; }

        virtual void process(modules::ModuleProcessor& processor) = 0;

        /// Draws the UI
        virtual void ui() {}

        virtual bool has_presets() { return false; } // TODO: actual preset system...

        virtual void save(std::ostream &data);
        virtual void load(std::istream &data);
    }; // class ModuleBase

    /**
    * Abstract class for a module host implementation that uses the ModuleBase class.
    **/
    class ModuleHost : public modules::ModuleHost
    {
    private:
        static void mod_process(modules::ModuleProcessor &proc);
        static std::unordered_map<modules::ModuleID, ModuleBase*> _modules;    
    
    protected:
        void init_module(ModuleBase *module, modules::ModuleCreator &creator);
    
    public:
        /**
        * Get the ModuleBase class associated with the module ID. The
        * pointer is owned by the audio engine, so if the module is destroyed
        * the pointer will be invalidated.
        * @param id The ID of the module.
        * @returns The pointer to the ModuleBase, or nullptr if not found.
        **/
        static ModuleBase* get_module(modules::ModuleID id);

        virtual void destroy_module(const std::string class_name, modules::ModuleID id, void *userdata) override;
    }; // class ModuleHost

    /**
    * Standardized structure used to send song/track events.
    **/
    struct TrackEvent
    {
        enum EventKind
        {
            /**
            * A key was released.
            **/
            NOTE_OFF,

            /**
            * A key was pressed.
            **/
            NOTE_ON,

            /**
            * Retrieve song tempo.
            **/
            TEMPO,
        };
        
        /**
        * The timestamp of the event relative to AudioEngine::frame_time.
        * It is expected that events are sent in sequential order.
        **/
        uint32_t timestamp;

        EventKind event_kind;
        
        union
        {
            struct
            {
                uint8_t key;
                uint8_t velocity;
            } note;
            
            float tempo;
        };

        static TrackEvent init_note_on(uint8_t key, float velocity);
        static TrackEvent init_note_off(uint8_t key, float velocity);
        static TrackEvent init_tempo(float tempo);
        static TrackEvent set_timestamp(uint32_t timestamp, const TrackEvent& event);
    }; // struct TrackEvent

    /**
    * Read track events, respecting their timestamp values.
    **/
    class TrackEventReader
    {
    private:
        TrackEvent _stored_event;
        modules::ModuleProcessor *proc;
        const unsigned int msg_index;
        unsigned int frame;
    public:
        TrackEventReader(const unsigned int input_port_index);

        /**
        * Called at the start of the process function.
        **/
        void new_run(modules::ModuleProcessor *proc);

        /**
        * Read the next event.
        * Use like: `while (event_reader.read(&event)) this->event(event);`
        * @param out_event Output event.
        * @returns True if there was an event on this frame, false if not.
        **/
        bool read(TrackEvent *out_event);
    }; // class TrackEventReader
} // namespace modx