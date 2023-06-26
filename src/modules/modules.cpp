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
    "effects.analyzer", "Analyzer",
    "effects.gain", "Gain",
    "effects.delay", "Echo/Delay",
});

std::array<audiomod::ModuleListing, NUM_INSTRUMENTS> audiomod::instruments_list({
    "synth.waveform", "Waveform"
});

#define MAP(id, class) if (mod_id == id) return new class(audio_dest)
ModuleBase* audiomod::create_module(const std::string& mod_id, DestinationModule& audio_dest, plugins::PluginManager& plugin_manager) {
    // synthesizers
    MAP("synth.waveform", WaveformSynth); // TODO: add fourth oscillator and allow FM modulation
    // TODO: harmonics synth
    // TODO: noise synth
    // TODO: noise spectrum synth
    // TODO: sampler synth
    // TODO: Slicer synth?

    // effects
    // TODO: should be "effect.X", not "effects.X" 
    MAP("effects.analyzer", AnalyzerModule);
    MAP("effects.volume", VolumeModule);
    MAP("effects.gain", GainModule);
    MAP("effects.delay", DelayModule);
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
            plugins::Plugin* plugin;

            switch (plugin_data.type)
            {
                case plugins::PluginType::Ladspa:
                    plugin = new plugins::LadspaPlugin(audio_dest, plugin_data);
                    break;

                case plugins::PluginType::Lv2:
                    plugin = new plugins::Lv2Plugin(audio_dest, plugin_data);
                    break;

                default:
                    throw std::runtime_error("invalid plugin type");
            }

            return new plugins::PluginModule(plugin);
        }
    }

    // no module found
    return nullptr;
}

#undef MAP