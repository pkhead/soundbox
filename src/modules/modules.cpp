#include "../audio.h"
#include "../sys.h"
#include "../plugins.h"
#include "../plugin_hosts/ladspa.h"
#include "../plugin_hosts/lv2.h"
#include "../ui/editor.h"
#include "modules.h"
#include <iostream>
#include <stdexcept>

using namespace audiomod;

std::array<audiomod::ModuleListing, NUM_EFFECTS> audiomod::effects_list({
    "effect.analyzer", "Analyzer",
    "effect.gain", "Gain",
    "effect.delay", "Echo/Delay",
});

std::array<audiomod::ModuleListing, NUM_INSTRUMENTS> audiomod::instruments_list({
    "synth.waveform", "Waveform"
});

#define MAP(id, class) if (mod_id == id) return new class(audio_dest)
ModuleBase* audiomod::create_module(
    const std::string& mod_id,
    DestinationModule& audio_dest,
    plugins::PluginManager& plugin_manager,
    WorkScheduler& scheduler
) {
    // synthesizers
    MAP("synth.waveform", WaveformSynth); // TODO: add fourth oscillator and allow FM modulation
    // TODO: harmonics synth
    // TODO: noise synth
    // TODO: noise spectrum synth
    // TODO: sampler synth
    // TODO: Slicer synth?

    // effects
    MAP("effect.analyzer", AnalyzerModule);
    MAP("effect.volume", VolumeModule);
    MAP("effect.gain", GainModule);
    MAP("effect.delay", DelayModule);
    // TODO: equalizer
    // TODO: distortion effect
    // TODO: bitcrusher/downsampler effect
    // TODO: chrous effect
    // TODO: flanger effect
    // TODO: phaser effect
    // TODO: compressor effect

    // create module for plugin
    for (auto& plugin_data : plugin_manager.get_plugin_data())
    {
        if (mod_id == plugin_data.id)
        {
            plugins::PluginModule* plugin;

            switch (plugin_data.type)
            {
                case plugins::PluginType::Ladspa:
                    plugin = new plugins::LadspaPlugin(audio_dest, plugin_data);
                    break;

                case plugins::PluginType::Lv2:
                try {
                    plugin = new plugins::Lv2Plugin(audio_dest, plugin_data, scheduler);
                } catch (plugins::lv2_error& err) {
                    throw module_create_error(err.what());
                }
                    break;

                default:
                    throw std::runtime_error("invalid plugin type");
            }

            return plugin;
        }
    }

    // no module found
    return nullptr;
}

bool audiomod::is_module_instrument(const std::string &mod_id, plugins::PluginManager& plugin_manager)
{
    size_t sep = mod_id.find_first_of(".");
    if (mod_id.substr(0, sep) == "synth")
        return true;

    if (mod_id.substr(0, sep) == "plugin")
    {
        for (auto& plugin_data : plugin_manager.get_plugin_data())
        {
            if (mod_id == plugin_data.id)
            {
                if (plugin_data.is_instrument) return true;
            }
        }
    }

    return false;
}

#undef MAP