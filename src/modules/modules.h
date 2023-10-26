#pragma once
#include "../audio.h"
#include "../dsp.h"

// Module/plugin lists
namespace audiomod
{
    struct ModuleListing
    {
        const char* id;
        const char* name;
    };

    constexpr size_t NUM_EFFECTS = 7;
    constexpr size_t NUM_INSTRUMENTS = 1;
    extern std::array<ModuleListing, NUM_EFFECTS> effects_list;
    extern std::array<ModuleListing, NUM_INSTRUMENTS> instruments_list;

    // define in modules/modules.cpp
    ModuleBase* create_module(
        const std::string& mod_id,
        DestinationModule& audio_dest,
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