#include "host.hpp"
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

modules::ModuleInfo mod_info(const std::string &id, const std::string &name, bool audio_input = true, bool midi_input = false)
{
    modules::ModuleInfo info = modules::ModuleInfo(id, name);
    info.has_audio_input = audio_input;
    info.has_midi_input = midi_input;
    info.author = "soundbox";
    return info;
}

const std::vector<modules::ModuleInfo> InternalModuleHost::scan_modules()
{
    std::vector<modules::ModuleInfo> list;
    list.push_back(mod_info("sbox::waveform", "Waveform Synth", false, true));
    list.push_back(mod_info("sbox::channel_controller", "sbox::channel_controller", false));
    list.push_back(mod_info("sbox::fader", "sbox::fader"));
    list.push_back(mod_info("sbox::gain", "Gain"));
    list.push_back(mod_info("sbox::analyzer", "Analyzer"));
    return list;
}

#define ASSOC_MODULE(strname, modclass) if (create.class_name == strname) { modx::ModuleBase *mod = new modclass(create); init_module(mod, create); return true; }

bool InternalModuleHost::create_module(modules::ModuleCreator &create)
{
    ASSOC_MODULE("sbox::waveform", WaveformModule);
    ASSOC_MODULE("sbox::channel_controller", ChannelControllerModule);
    ASSOC_MODULE("sbox::fader", FaderModule);
    ASSOC_MODULE("sbox::gain", GainModule);
    ASSOC_MODULE("sbox::analyzer", AnalyzerModule);
    return false;
}