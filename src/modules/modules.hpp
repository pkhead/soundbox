/**
* Extensions/implementations for the audio engine.
*/
#pragma once
#include <memory>
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

        std::string class_name() const { return _engine.module_class_name(_id); }

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

        template <typename T>
        T control_get_value(unsigned int index) { return _engine.control_get_value<T>(_id, index); }
        
        template <typename T>
        bool control_set_value(unsigned int index, T value) { return _engine.control_set_value(_id, index, value); }

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
    public:
        ModuleBase(modules::ModuleCreator& creator) : _id(creator.id), engine(&creator.engine) {}
        virtual ~ModuleBase() {}

        modules::AudioEngine *const engine;
        inline modules::ModuleID id() const { return _id; }
        virtual void process(modules::ModuleProcessor& processor) = 0;
        virtual void ui() {}
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

        virtual void destroy_module(const std::string class_name, void *userdata) override;
    }; // class ModuleHost
} // namespace modx