#include "delay.hpp"
#include "audio_engine/audio_engine.hpp"
#include "module_hosts/modules.hpp"

using namespace hosts::internal;

DelayModule::DelayModule(modules::ModuleCreator &create) :
    modx::ModuleBase(create)
{
    create.add_audio_input(2);
    create.add_audio_output(2);

    create.add_control<float>(CONTROL_FEEDBACK, "Feedback", 0.5f);
    create.add_control<float>(CONTROL_MIX, "Wet/Dry Mix", 0.5f);
    create.add_control<float>(CONTROL_DELAY_SECS_L, "Delay L (secs)", 0.3f);
    create.add_control<int>(CONTROL_DELAY_DIV_L, "Delay L (tempo division)", 0);
    create.add_control<float>(CONTROL_DELAY_SECS_R, "Delay R (secs)", 0.3f);
    create.add_control<int>(CONTROL_DELAY_DIV_R, "Delay R (tempo division)", 0);
    create.add_control<bool>(CONTROL_USE_TEMPO, "Use Tempo", false);
    create.add_control<bool>(CONTROL_STEREO_LOCK, "Stereo Lock", false);
}

void DelayModule::process(modules::ModuleProcessor &proc) {
    float feedback = proc.get_control_value<float>(CONTROL_FEEDBACK);
    float mix = proc.get_control_value<float>(CONTROL_MIX);

    //proc.
}

void DelayModule::ui() {
    
}