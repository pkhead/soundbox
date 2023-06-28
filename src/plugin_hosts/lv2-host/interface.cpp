#include <cmath>

#include "lv2interface.h"
#include "../../util.h"

using namespace lv2;
using namespace plugins;

Lv2Plugin::Lv2Plugin(
    audiomod::DestinationModule& dest,
    const PluginData& data,
    WorkScheduler& scheduler
) : PluginModule(dest, data), host(dest, data, scheduler), ui_host(&host)
{
    _has_interface = control_value_count() > 0 || ui_host.has_custom_ui();
    host.start();
}

Lv2Plugin::~Lv2Plugin()
{
    host.stop();
}

int Lv2Plugin::control_value_count() const
{
    return host.input_displays.size();
}

int Lv2Plugin::output_value_count() const
{
    return host.output_displays.size();
}

PluginModule::ControlValue Lv2Plugin::get_control_value(int index)
{
    ControlValue value;
    InterfaceDisplay& display_info = host.input_displays[index];

    if (display_info.is_parameter) {
        Parameter* param = display_info.param;

        value.name = param->label.c_str();
        value.is_integer = param->type == Parameter::TypeInt || param->type == Parameter::TypeLong;
        value.is_logarithmic = false;
        value.is_sample_rate = false;
        value.is_toggle = param->type == Parameter::TypeBool;

        // todo: find min and max
        value.has_default = false;
        value.min = -10.0f;
        value.max = 10.0f;

        switch (param->type) {
            case Parameter::TypeFloat:
                value.value = param->v._float;
                value.format = "%.3f";
                break;
            
            case Parameter::TypeDouble:
                value.value = param->v._double;
                value.format = "%.3f";
                break;

            case Parameter::TypeInt:
                value.value = param->v._int;
                value.format = "%i";
                break;

            case Parameter::TypeLong:
                value.value = param->v._long;
                value.format = "%li";
                break;

            case Parameter::TypeBool:
                value.value = param->v._bool ? 1.0f : -1.0f;
                break;

            default:
                DBGBREAK;

        }
    } else {
        ControlInputPort* impl = display_info.port_in;

        value.name = impl->name.c_str();
        value.value = impl->value;
        value.has_default = true;
        value.default_value = impl->default_value;
        value.max = impl->max;
        value.min = impl->min;
        value.is_integer = false;
        value.is_logarithmic = false;
        value.is_sample_rate = false;
        value.is_toggle = false;

        switch (impl->unit)
        {
            case UnitType::dB:
                value.format = "%.3f dB";
                break;

            default:
                value.format = "%.3f";
                break;
        }
    }

    return value;
}

void Lv2Plugin::set_control_value(int index, float value)
{
    InterfaceDisplay& display_info = host.input_displays[index];

    if (display_info.is_parameter) {
        Parameter* param = display_info.param;

        switch (param->type) {
            case Parameter::TypeFloat:
                param->v._float = value;
                break;
            
            case Parameter::TypeDouble:
                param->v._double = value;
                break;

            case Parameter::TypeInt:
                param->v._int = roundf(value);
                break;

            case Parameter::TypeLong:
                param->v._long = roundf(value);
                break;

            case Parameter::TypeBool:
                param->v._bool = value >= 0.0f;
                break;

            default:
                DBGBREAK;

        }
    } else {
        ControlInputPort* impl = display_info.port_in;
        impl->value = value;
    }
}

PluginModule::OutputValue Lv2Plugin::get_output_value(int index)
{
    OutputValue value;
    InterfaceDisplay& display = host.output_displays[index];

    static char display_str[64];

    if (display.is_parameter) {
        Parameter* param = display.param;

        // TODO: parameter types other than float
        switch (param->type) {
            case Parameter::TypeFloat:
                snprintf(display_str, 64, "%f", param->v._float);
                break;
            
            case Parameter::TypeDouble:
                snprintf(display_str, 64, "%f", param->v._double);
                break;

            case Parameter::TypeInt:
                snprintf(display_str, 64, "%i", param->v._int);
                break;

            case Parameter::TypeLong:
                snprintf(display_str, 64, "%li", param->v._long);
                break;

            case Parameter::TypeBool:
                snprintf(display_str, 64, "%s", param->v._bool ? "yes": "no");
                break;

            default:
                DBGBREAK;
        }

        value.name = param->label.c_str();
    } else {
        ControlOutputPort* impl = display.port_out;
        value.name = impl->name.c_str();
    }

    value.value = display_str;

    return value;
}

void Lv2Plugin::control_value_change(int index)
{
    InterfaceDisplay& display = host.input_displays[index];

    if (display.is_parameter) {
        display.param->did_change = true;
    }
}

bool Lv2Plugin::show_interface() {
    bool status;
    if ((status = PluginModule::show_interface())) {
        ui_host.show();
    }
    return status;
}

void Lv2Plugin::hide_interface() {
    _interface_shown = false;
    ui_host.hide();
}

// override render_interface to not show an ImGui window
// if it is showing a non-embedded plugin ui
bool Lv2Plugin::render_interface() {
    if (!_interface_shown) return false;

    if (ui_host.has_custom_ui()) {
        return ui_host.render();
    } else {
        return PluginModule::render_interface();
    }
}

void Lv2Plugin::event(const audiomod::MidiEvent* event) {
    return host.event(event);
}

size_t Lv2Plugin::receive_events(void** handle, audiomod::MidiEvent* buffer, size_t capacity) {
    return host.receive_events(handle, buffer, capacity);
}

void Lv2Plugin::flush_events() {
    return host.flush_events();
}

void Lv2Plugin::process(
    float** inputs,
    float* output,
    size_t num_inputs,
    size_t buffer_size,
    int sample_rate,
    int channel_count
) {
    host.song = song;
    return host.process(inputs, output, num_inputs, buffer_size, sample_rate, channel_count);
}