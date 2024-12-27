#include <cassert>
#include <cstdint>
#include <widgets.hpp>
#include <util.hpp>
#include "audio_engine/audio_engine.hpp"
#include "imguiext/imgui-knobs.h"
#include "modules.hpp"
#include "../log.hpp"

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

void ModuleHost::destroy_module(const std::string class_name, modules::ModuleID id, void* userdata)
{
    ModuleBase* mod = static_cast<ModuleBase*>(userdata);
    assert(mod->id() == id);
    assert(_modules.find(mod->id()) != _modules.end());
    _modules.erase(mod->id());
    delete mod;
}

void ModuleHost::mod_process(modules::ModuleProcessor& process)
{
    ModuleBase* mod = static_cast<ModuleBase*>(process.userdata);
    mod->process(process);
}




//////////////////
// track events //
//////////////////
TrackEvent TrackEvent::init_note_on(uint8_t key, float velocity)
{
    velocity = util::clamp<float>(0.0f, 1.0f, velocity);
    TrackEvent event{};
    event.event_kind = NOTE_ON;
    event.timestamp = 0;
    event.note.key = key;
    event.note.velocity = (uint8_t)(velocity * UINT8_MAX);
    return event;
}

TrackEvent TrackEvent::init_note_off(uint8_t key, float velocity)
{
    velocity = util::clamp<float>(0.0f, 1.0f, velocity);
    TrackEvent event{};
    event.event_kind = NOTE_OFF;
    event.timestamp = 0;
    event.note.key = key;
    event.note.velocity = (uint8_t)(velocity * UINT8_MAX);
    return event;
}

TrackEvent TrackEvent::init_tempo(float tempo)
{
    TrackEvent event{};
    event.event_kind = TEMPO;
    event.timestamp = 0;
    event.tempo = tempo;
    return event;
}

TrackEvent TrackEvent::set_timestamp(uint32_t timestamp, const TrackEvent &event)
{
    TrackEvent ret = event;
    ret.timestamp = timestamp;
    return ret;
}







TrackEventReader::TrackEventReader(unsigned int input_port_index) :
    msg_index(input_port_index)
{
    proc = nullptr;
}

void TrackEventReader::new_run(modules::ModuleProcessor *p_proc)
{
    frame = 0;
    this->proc = p_proc;

    unsigned int read = proc->read_message(msg_index, &_stored_event, sizeof(_stored_event));
    if (read == 0)
    {
        _stored_event.timestamp = UINT32_MAX;
        return;
    }
}

bool TrackEventReader::read(TrackEvent *out_event)
{
    if (frame < _stored_event.timestamp)
    {
        frame++;
        return false;
    }

    *out_event = _stored_event;

    unsigned int read = proc->read_message(msg_index, &_stored_event, sizeof(_stored_event));
    if (read == 0)
    {
        _stored_event.timestamp = UINT32_MAX;
    }

    return true;
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
    float v = engine->control_get_value<float>(id(), control_index);
    if (widgets::knob(label, &v, v_min, v_max, fmt, flags, speed, variant, size, steps))
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
        size = ImGui::GetFontSize() * 2.5f;
    }
    
    int v = engine->control_get_value<int>(id(), control_index);
    if (widgets::knob_int(label, &v, v_min, v_max, fmt, flags, speed, variant, size, steps))
    {
        engine->control_set_value<int>(id(), control_index, v);
        return true;
    }

    return false;
}