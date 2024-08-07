#include <cassert>
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