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

const std::vector<std::string> InternalModuleHost::scan_modules()
{
    std::vector<std::string> list;
    list.push_back("sbox::osc");
    list.push_back("sbox::midi_in");
    list.push_back("sbox::fader");
    list.push_back("sbox::gain");
    return list;
}

#define ASSOC_MODULE(strname, modclass) if (create.class_name == strname) { modx::ModuleBase *mod = new modclass(create); init_module(mod, create); return true; }

bool InternalModuleHost::create_module(modules::ModuleCreator &create)
{
    ASSOC_MODULE("sbox::osc", OscModule);
    ASSOC_MODULE("sbox::midi_in", MidiInputModule);
    ASSOC_MODULE("sbox::fader", FaderModule);
    ASSOC_MODULE("sbox::gain", GainModule);
    return false;
}