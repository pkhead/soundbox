#include "host.hpp"
#include "modules.hpp"

using namespace hosts::internal;

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

const std::vector<modules::ModuleInfo> InternalModuleHost::scan_modules()
{
    std::vector<modules::ModuleInfo> list;
    list.push_back(modules::ModuleInfo("sbox::waveform", "Waveform Synth", false));
    list.push_back(modules::ModuleInfo("sbox::channel_controller", "Channel Controller", false));
    list.push_back(modules::ModuleInfo("sbox::fader", "Fader"));
    list.push_back(modules::ModuleInfo("sbox::gain", "Gain"));
    list.push_back(modules::ModuleInfo("sbox::analyzer", "Analyzer"));
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