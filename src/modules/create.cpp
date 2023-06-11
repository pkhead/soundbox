// implementations in modules are written in header files
// because i don't want to create two files for every module
// instead i write the implementation in the header file, and
// compile the implementations in a singular modules.cpp file
#include "modules/delay.h"
#define MODULE_IMPL

#include "../audio.h"
#include "modules.h"

using namespace audiomod;

std::array<audiomod::ModuleListing, NUM_EFFECTS> audiomod::effects_list({
    "effects.analyzer", "Analyzer",
    "effects.gain", "Gain",
    "effects.delay", "Echo/Delay",
});

std::array<audiomod::ModuleListing, NUM_INSTRUMENTS> audiomod::instruments_list({
    "synth.waveform", "Waveform"
});

#define MAP(id, class) if (mod_id == id) return new class(song)
ModuleBase* audiomod::create_module(const std::string& mod_id, Song* song) {
    // synthesizers
    MAP("synth.waveform", WaveformSynth); // TODO: add fourth oscillator and allow FM modulation
    // TODO: harmonics synth
    // TODO: noise synth
    // TODO: noise spectrum synth
    // TODO: sampler synth
    // TODO: Slicer synth?

    // effects
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

    return nullptr;
}

#undef MAP