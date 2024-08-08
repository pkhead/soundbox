#include <cassert>
#include "imguiext/imgui-knobs.h"
#include "modules.hpp"

using namespace modx;

ModuleHandle::ModuleHandle(modules::AudioEngine &engine, const std::string &class_name) :
    _engine(engine)
{
    _id = _engine.create_module(class_name);

    if (_id != 0)
        logger::log_debug("create module id %i (class %s)", _id, class_name.c_str());
}

ModuleHandle::~ModuleHandle()
{
    if (_id == 0) return;
    
    logger::log_debug("destroy module id %i (class %s)", _id, _engine.module_class_name(_id).c_str());
    _engine.destroy_module(_id);
}

std::unordered_map<modules::ModuleID, ModuleBase*> ModuleHost::_modules = std::unordered_map<modules::ModuleID, ModuleBase*>();

ModuleBase* ModuleHost::get_module(modules::ModuleID id)
{
    const auto &it = _modules.find(id);
    if (it == _modules.end()) return nullptr;
    return _modules[id];
}

void ModuleHost::init_module(ModuleBase* module, modules::ModuleCreator &creator)
{
    creator.userdata = module;
    creator.processor = mod_process;
    _modules[creator.id] = module;
}

void ModuleHost::destroy_module(const std::string class_name, void* userdata)
{
    ModuleBase* mod = static_cast<ModuleBase*>(userdata);
    assert(_modules.find(mod->id()) != _modules.end());
    _modules.erase(mod->id());
    delete mod;
}

void ModuleHost::mod_process(modules::ModuleProcessor& process)
{
    ModuleBase* mod = static_cast<ModuleBase*>(process.userdata);
    mod->process(process);
}







////////////////
// ui helpers //
////////////////

bool ModuleBase::ui_knob(
    const char *label,
    unsigned int control_index,
    float v_min,
    float v_max,
    const char *fmt,
    ImGuiKnobFlags flags,
    float speed,
    ImGuiKnobVariant variant,
    float size ,
    int steps
)
{
    if (size == 0.0f)
    {
        size = ImGui::GetFontSize() * 3.0f;
    }

    float v = engine->control_get_value<float>(id(), control_index);
    if (ImGuiKnobs::Knob(label, &v, v_min, v_max, speed, fmt, variant, size, flags, steps))
    {
        engine->control_set_value<float>(id(), control_index, v);
        return true;
    }

    return false;
}

bool ModuleBase::ui_knob_int(
    const char *label,
    unsigned int control_index,
    int v_min,
    int v_max,
    const char *fmt,
    ImGuiKnobFlags flags,
    float speed,
    ImGuiKnobVariant variant,
    float size,
    int steps
)
{
    if (size == 0.0f)
    {
        size = ImGui::GetFontSize() * 3.0f;
    }
    
    int v = engine->control_get_value<int>(id(), control_index);
    if (ImGuiKnobs::KnobInt(label, &v, v_min, v_max, speed, fmt, variant, size, flags, steps))
    {
        engine->control_set_value<int>(id(), control_index, v);
        return true;
    }

    return false;
}