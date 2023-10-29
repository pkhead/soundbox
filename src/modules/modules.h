#pragma once
#include "../audio.h"
#include "../dsp.h"
#include "../worker.h"

// Module/plugin lists
namespace audiomod
{
    /**
    * A class which handles an effect rack
    **/
    class EffectsRack
    {
    protected:
        std::vector<ModuleNodeRc> inputs;
        ModuleNodeRc output;

    public:
        EffectsRack();
        ~EffectsRack();
        
        std::vector<ModuleNodeRc> modules;

        void insert(ModuleNodeRc& module, size_t position);
        void insert(ModuleNodeRc& module);
        ModuleNodeRc remove(size_t position);

        /**
        * Connect a module to the effects rack
        */
        void connect_input(ModuleNodeRc& input);

        /**
        * Connect the effects rack to a module
        * @returns The previous output module
        */
        ModuleNodeRc connect_output(ModuleNodeRc& output);

        /**
        * Disconnect the input from effects rack
        */
        bool disconnect_input(ModuleNodeRc& input);
        void disconnect_all_inputs();

        /**
        * Disconnect the effects rack to its output
        * @returns A reference to the now disconnected output module
        */
        ModuleNodeRc disconnect_output();
    };

    class FXBus
    {
    public:
        FXBus(ModuleContext& modctx);
        ~FXBus();

        EffectsRack rack;

        class FaderModule : public ModuleBase
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

            FaderModule()
            : ModuleBase(false)
            {}
        };
        ModuleNodeRc controller;

        inline std::vector<ModuleNodeRc>& get_modules()
        {
            return rack.modules;
        };

        static constexpr size_t name_capacity = 16;
        char name[16];

        bool interface_open = false;
        bool solo = false;

        // index of target bus
        int target_bus = 0;

        inline void insert(ModuleNodeRc& module, size_t position) { rack.insert(module, position); };
        inline void insert(ModuleNodeRc& module) { rack.insert(module); };
        inline ModuleNodeRc remove(size_t position) { return rack.remove(position); };
        
        // wrap around EffectsRack methods, because
        // EffectsRack's output should always be the controller module
        inline void connect_input(ModuleNodeRc& input) { rack.connect_input(input); };
        inline bool disconnect_input(ModuleNodeRc& input) { return rack.disconnect_input(input); };
        inline void disconnect_all_inputs() { rack.disconnect_all_inputs(); };

        // these implementations aren't one-liners so maybe inlining isn't a good idea
        ModuleNodeRc connect_output(ModuleNodeRc& output);
        ModuleNodeRc disconnect_output();
    };

    struct ModuleListing
    {
        const char* id;
        const char* name;
    };

    constexpr size_t NUM_EFFECTS = 7;
    constexpr size_t NUM_INSTRUMENTS = 2;
    extern std::array<ModuleListing, NUM_EFFECTS> effects_list;
    extern std::array<ModuleListing, NUM_INSTRUMENTS> instruments_list;

    // define in modules/modules.cpp
    ModuleNodeRc create_module(
        const std::string& mod_id,
        ModuleContext& modctx,
        plugins::PluginManager& plugin_manager,
        WorkScheduler& work_scheduler
    );

    // if module takes in MIDI inputs or does not take in audio input
    bool is_module_instrument(const std::string& mod_id, plugins::PluginManager& plugin_manager);
}

#include "analyzer.h"
#include "gain.h"
#include "volume.h"
#include "waveform.h"
#include "delay.h"
#include "eq.h"
#include "limiter.h"
#include "compressor.h"
#include "reverb.h"
#include "omnisynth.h"