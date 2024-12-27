#include "host.hpp"
#include "audio_engine/audio_engine.hpp"
#include "module_hosts/internal/mod/delay.hpp"
#include "modules.hpp"

using namespace hosts::internal;

const std::array<std::string, 2> InternalModuleHost::hidden_mod_classes = {
    "sbox::channel_controller",
    "sbox::fader"
};

//////////////////////////
// Internal Module Host //
//////////////////////////
bool InternalModuleHost::initialize()
{
    // no-op
    return true;
}

const char* InternalModuleHost::host_id() const
{
    return "sbox";
}

// helper macros
#define DECLARE_MODULE(class_name, name, audio_input, audio_output, midi_input, midi_output)\
    list.push_back(modules::ModuleInfo { class_name, name, "soundbox", audio_input, audio_output, midi_input, midi_output })
#define DECLARE_AUDIO_MODULE(class_name, name)\
    DECLARE_MODULE(class_name, name, 0, 0, -1, -1)
#define DECLARE_INSTRUMENT_MODULE(class_name, name)\
    DECLARE_MODULE(class_name, name, -1, 0, 0, -1)

const std::vector<modules::ModuleInfo> InternalModuleHost::scan_modules()
{
    std::vector<modules::ModuleInfo> list;
    
    // instruments
    DECLARE_INSTRUMENT_MODULE("sbox::waveform", "Waveform Synth");

    // effects/audio modules
    DECLARE_AUDIO_MODULE("sbox::gain", "Gain");
    DECLARE_AUDIO_MODULE("sbox::analyzer", "Analyzer");
    DECLARE_MODULE("sbox::delay", "Delay", 0, 0, 1, -1);

    DECLARE_AUDIO_MODULE("sbox::mono_to_stereo", "Mono -> Stereo");
    DECLARE_AUDIO_MODULE("sbox::stereo_to_mono", "Stereo -> Mono");

    // misc
    DECLARE_MODULE("sbox::channel_controller", "sbox::channel_controller", -1, -1, -1, 0);
    DECLARE_AUDIO_MODULE("sbox::fader", "sbox::fader");

    return list;
}

#undef DECLARE_MODULE
#undef DECLARE_AUDIO_MODULE
#undef DECLARE_INSTRUMENT_MODULE

#define ASSOC_MODULE(strname, modclass) if (create.class_name == strname) { modx::ModuleBase *mod = new modclass(create); init_module(mod, create); return true; }

bool InternalModuleHost::create_module(modules::ModuleCreator &create)
{
    ASSOC_MODULE("sbox::waveform", WaveformModule);
    
    ASSOC_MODULE("sbox::gain", GainModule);
    ASSOC_MODULE("sbox::analyzer", AnalyzerModule);
    ASSOC_MODULE("sbox::delay", DelayModule);

    ASSOC_MODULE("sbox::mono_to_stereo", MonoToStereo);
    ASSOC_MODULE("sbox::stereo_to_mono", StereoToMono);
    ASSOC_MODULE("sbox::channel_controller", ChannelControllerModule);
    ASSOC_MODULE("sbox::fader", FaderModule);
    return false;
}