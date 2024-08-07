#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <math.h>
#include <sys/types.h>
#include "../sys.h"
#include "../audio.h"
#include "delay.h"
#include "../song.h"

using namespace audiomod;

DelayModule::DelayModule(ModuleContext& modctx)
:   ModuleBase(true),
    msg_queue(sizeof(module_state_t), 2)
{
    id = "effect.delay";
    name = "Delay";

    for (int i = 0; i < 2; i++)
    {
        _delay_time[i] = 0.25;
        _tempo_division[i] = 0;
    }

    _delay_mode = false;
    _feedback = 0.6f;
    _mix = 0.0f;
    _lock = true;

    process_state.delay_time[0] = _delay_time[0];
    process_state.delay_time[1] = _delay_time[1];
    process_state.feedback = _feedback;
    process_state.mix = _mix;

    size_t delay_line_size = (size_t)(modctx.sample_rate * 5); // maximum delay of 5 seconds
    delay_line[0].resize(delay_line_size);
    delay_line[1].resize(delay_line_size);
}

static double division_to_secs(float tempo, int division_enum)
{
    int exponent = division_enum / 3 - 6;
    int div_type = division_enum % 3;
    float len;

    if (div_type == 0) // normal beats
        len = powf(2.0f, exponent);
    else if (div_type == 1) // dotted beats
    {
        len = powf(2.0f, exponent);
        len += len / 2.0f;
    }
    else if (div_type == 2) // triplets
        len = powf(3.0f / 2.0f, exponent);

    return len * (60.0f / tempo);
}

void DelayModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count)
{
    if (this->panic)
    {
        this->panic = false;

        delay_line[0].clear();
        delay_line[1].clear();
    }

    // receive module state
    {
        auto handle = msg_queue.read();
        if (handle.size() > 0)
        {
            assert(handle.size() == sizeof(module_state_t));
            handle.read(&process_state, sizeof(module_state_t));
        }    
    }

    float delay_time_left = process_state.delay_time[0];
    float delay_time_right = process_state.delay_time[1];

    // convert time sample index from sample rate
    float delay_in_samples[2];
    delay_in_samples[0] = delay_time_left * sample_rate;
    delay_in_samples[1] = delay_time_right * sample_rate;

    float wet_mix = (process_state.mix + 1.0f) / 2.0f;
    float dry_mix = 1.0f - wet_mix;

    float input_frame[2];
    float delay_frame[2];

    delay_line[0].delay = delay_in_samples[0];
    delay_line[1].delay = delay_in_samples[1];

    for (size_t i = 0; i < buffer_size; i += 2)
    {
        delay_frame[0] = delay_line[0].read();
        delay_frame[1] = delay_line[1].read();

        // get data from inputs
        input_frame[0] = 0.0f;
        input_frame[1] = 0.0f;
        for (size_t j = 0; j < num_inputs; j++)
        {
            input_frame[0] += inputs[j][i];
            input_frame[1] += inputs[j][i + 1];
        }

        // mix dry and wet mix and write to output
        output[i] = (input_frame[0] * dry_mix) + (delay_frame[0] * wet_mix);
        output[i + 1] = (input_frame[1] * dry_mix) + (delay_frame[1] * wet_mix);

        // update delay line
        delay_line[0].write(process_state.feedback * (delay_frame[0] + input_frame[0]));
        delay_line[1].write(process_state.feedback * (delay_frame[1] + input_frame[1]));
    }
}

static const char* DIVISION_NAMES[] = {
    "1/64", // beats
    "1/64 dotted", // dotted
    "1/48", // triplet
    "1/32", // beats
    "1/32 dotted", // dotted
    "1/24", // triplet
    "1/16", // beats
    "1/16 dotted", // dotted
    "1/12", // triplet
    "1/8", // beats
    "1/8 dotted", // dotted
    "1/6", // triplet
    "1/4", // beats
    "1/4 dotted", // dotted
    "1/3", // triplet
    "1/2", // beats
    "1/2 dotted", // dotted
    "2/3", // triplet
    "1/1", // beats
    "1/1 dotted", // dotted
    "3/3", // triplet
    "2/1", // beats
    "2/1 dotted", // dotted
    "6/3", // triplet
    "4/1", // beats
    "4/1 dotted", // dotted
    "12/3", // triplet
    "8/1",  // beats
    "8/1 dotted", // dotted
    "24/3", // triplet
};

void DelayModule::_interface_proc()
{
    float mix = _mix;
    float feedback = _feedback;
    bool delay_mode = _delay_mode;
    
    float delay_time[2];
    delay_time[0] = _delay_time[0];
    delay_time[1] = _delay_time[1];

    int tempo_division[2];
    tempo_division[0] = _tempo_division[0];
    tempo_division[1] = _tempo_division[1];
    
    // labels
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Dry/Wet Mix");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Feedback");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Delay L");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Delay R");
    ImGui::EndGroup();

    // values
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushItemWidth(ImGui::GetTextLineHeight() * 14.0f);

    // dry/wet
    ImGui::SliderFloat("###mix", &mix, -1.0f, 1.0f);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
        mix = 0.0f;

    // feedback
    ImGui::SliderFloat("###feedback", &feedback, 0.0f, 1.0f);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
        feedback = 0.5f;

    for (int c = 0; c < 2; c++)
    {
        ImGui::PushID(c);

        if (delay_mode)
        {
            // delay time in beats
            int temp = tempo_division[c];
            ImGui::SliderInt("###tempo_division", &temp, 0, (10 * 3) - 1, DIVISION_NAMES[temp]);
            tempo_division[c] = temp;
        }
        else
        {
            // delay time in seconds
            float delay_time_float = delay_time[c];
            ImGui::SliderFloat("###delay", &delay_time_float, 0.0f, 4.0f);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
                delay_time_float = 0.2f;
            delay_time[c] = delay_time_float;
        }

        // channel lock behavior
        if (_lock && (ImGui::IsItemActive() || ImGui::IsItemClicked(ImGuiMouseButton_Middle)))
        {
            tempo_division[1-c] = tempo_division[c];
            delay_time[1-c] = delay_time[c];
        }

        ImGui::PopID();
    }

    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Use Tempo");
    ImGui::SameLine();
    if (ImGui::Checkbox("###mode", &delay_mode))
    {
        // TODO: set tempo_division to nearest delay_mode
        if (delay_mode)
        {

        }
        else
        {
            delay_time[0] = division_to_secs(song->tempo, tempo_division[0]);
            delay_time[1] = division_to_secs(song->tempo, tempo_division[1]);
        }
    }
    ImGui::SameLine();
    ImGui::Text("Lock");
    ImGui::SameLine();
    if (ImGui::Checkbox("##lock", &_lock) && _lock)
    {
        delay_time[1] = delay_time[0];
        tempo_division[1] = tempo_division[0];
    }

    this->_delay_mode = delay_mode;
    this->_mix = mix;
    this->_feedback = feedback;
    this->_delay_time[0] = delay_time[0];
    this->_delay_time[1] = delay_time[1];
    this->_tempo_division[0] = tempo_division[0];
    this->_tempo_division[1] = tempo_division[1];

    // send state to processing thread
    module_state_t state;
    state.delay_time[0] = _delay_mode ? division_to_secs(song->tempo, _tempo_division[0]) : _delay_time[0];
    state.delay_time[1] = _delay_mode ? division_to_secs(song->tempo, _tempo_division[1]) : _delay_time[1];
    state.feedback = _feedback;
    state.mix = _mix;

    msg_queue.post(&state, sizeof(state));
}

void DelayModule::save_state(std::ostream& ostream)
{
    push_bytes<uint8_t>(ostream, 0); // write version

    bool delay_mode = this->_delay_mode;
    push_bytes<uint8_t>(ostream, delay_mode);
    
    for (int c = 0; c < 2; c++)
    {
        if (delay_mode)
            push_bytes<uint32_t>(ostream, _tempo_division[c]);
        else
            push_bytes<float>(ostream, _delay_time[c]);
    }

    push_bytes<float>(ostream, _feedback);
    push_bytes<float>(ostream, _mix);
}

bool DelayModule::load_state(std::istream& istream, size_t size)
{ 
    uint8_t version = pull_bytesr<uint8_t>(istream);
    if (version != 0) return false;
    
    bool delay_mode = pull_bytesr<uint8_t>(istream);
    _delay_mode = delay_mode;
    
    for (int c = 0; c < 2; c++)
    {
        if (delay_mode)
            _tempo_division[c] = pull_bytesr<uint32_t>(istream);
        else
            _delay_time[c] = pull_bytesr<float>(istream);

        // in case of erroneous tempo division
        // really i only check this because i changed the parameters of the delay ui
        // one day, and was too lazy to make a new version #
        if (_tempo_division[c] >= sizeof(DIVISION_NAMES) / sizeof(*DIVISION_NAMES))
        {
            _tempo_division[c] = 0;
        }
    }

    _feedback = pull_bytesr<float>(istream);
    _mix = pull_bytesr<float>(istream);

    // send state to processing thread
    module_state_t state;
    state.delay_time[0] = _delay_mode ? division_to_secs(song->tempo, _tempo_division[0]) : _delay_time[0];
    state.delay_time[1] = _delay_mode ? division_to_secs(song->tempo, _tempo_division[1]) : _delay_time[1];
    state.feedback = _feedback;
    state.mix = _mix;

    msg_queue.post(&state, sizeof(state));

    return true;
}