#include "../audio.h"
#include "../sys.h"
#include "../plugins.h"
#include "../editor/editor.h"
#include "modules.h"
#include <iostream>
#include <stdexcept>

using namespace audiomod;

std::array<audiomod::ModuleListing, NUM_EFFECTS> audiomod::effects_list({
    "effect.analyzer", "Analyzer",
    "effect.gain", "Gain",
    "effect.delay", "Echo/Delay",
    "effect.eq", "Equalizer",
    "effect.limiter", "Limiter",
    "effect.compressor", "Compressor",
    "effect.reverb", "Reverb"
});

std::array<audiomod::ModuleListing, NUM_INSTRUMENTS> audiomod::instruments_list({
    "synth.waveform", "Waveform",
    "synth.omnisynth", "Omnisynth"
});

#define MAP(id, class) if (mod_id == id) return modctx.create<class>(modctx)
ModuleNodeRc audiomod::create_module(
    const std::string& mod_id,
    ModuleContext& modctx,
    plugins::PluginManager& plugin_manager,
    WorkScheduler& scheduler
) {
    // synthesizers
    MAP("synth.waveform", WaveformSynth); // TODO: add fourth oscillator and allow FM modulation
    MAP("synth.omnisynth", OmniSynth);

    // TODO: harmonics synth
    // TODO: noise synth
    // TODO: noise spectrum synth
    // TODO: sampler synth
    // TODO: Slicer synth?

    // effects
    MAP("effect.analyzer", AnalyzerModule);
    MAP("effect.volume", VolumeModule);
    MAP("effect.gain", GainModule);
    MAP("effect.delay", DelayModule);
    MAP("effect.eq", EQModule);
    MAP("effect.limiter", LimiterModule);
    MAP("effect.compressor", CompressorModule);
    MAP("effect.reverb", ReverbModule);

    // TODO: equalizer
    // TODO: distortion effect
    // TODO: bitcrusher/downsampler effect
    // TODO: chrous effect
    // TODO: flanger effect
    // TODO: phaser effect
    // TODO: compressor effect

    // create module for plugin
    for (auto& plugin_data : plugin_manager.get_plugin_data())
    {
        if (mod_id == plugin_data.id)
            return plugin_manager.instantiate_plugin(plugin_data, modctx, scheduler);
    }

    // no module found
    return nullptr;
}

bool audiomod::is_module_instrument(const std::string &mod_id, plugins::PluginManager& plugin_manager)
{
    size_t sep = mod_id.find_first_of(".");
    if (mod_id.substr(0, sep) == "synth")
        return true;

    if (mod_id.substr(0, sep) == "plugin")
    {
        for (auto& plugin_data : plugin_manager.get_plugin_data())
        {
            if (mod_id == plugin_data.id)
            {
                if (plugin_data.is_instrument) return true;
            }
        }
    }

    return false;
}

#undef MAP







//////////////////////
//   EFFECTS RACK   //
//////////////////////
EffectsRack::EffectsRack() : output(nullptr)
{}

EffectsRack::~EffectsRack() {
    for (ModuleNodeRc& input : inputs)
        input->disconnect();
    
    if (!modules.empty()) modules.back()->disconnect();
}

void EffectsRack::insert(ModuleNodeRc& module) {
    if (modules.empty()) return insert(module, 0);
    return insert(module, modules.size());
}

void EffectsRack::insert(ModuleNodeRc& module, size_t position) {
    // if there is nothing in the effects rack, this is a simple operation
    if (modules.empty()) {
        for (ModuleNodeRc& input : inputs)
        {
            input->disconnect();
            input->connect(module);
        }
        
        if (output != nullptr) {
            module->connect(output);
        } 

    // insertion at beginning
    } else if (position == 0) {
        for (ModuleNodeRc& input : inputs)
        {
            input->disconnect();
            input->connect(module);
        }
        
        module->connect(modules[0]);

    // insertion at end
    } else if (position == modules.size()) {
        modules.back()->disconnect();
        modules.back()->connect(module);
        if (output != nullptr) module->connect(output);
        
    // insertion at middle
    } else {
        ModuleNodeRc& prev = modules[position - 1];
        ModuleNodeRc& next = modules[position];
        
        prev->disconnect();
        prev->connect(module);

        module->connect(next);
    }

    // now, add module to the array
    modules.insert(modules.begin() + position, module);
}

ModuleNodeRc EffectsRack::remove(size_t position) {
    if (modules.empty()) return nullptr;

    ModuleNodeRc target_module = modules[position];
    target_module->remove_all_connections();

    // if there is only one item in the rack
    if (modules.size() == 1) {
        for (ModuleNodeRc& input : inputs)
            input->connect(output);
        
    // remove the first module
    } else if (position == 0) {
        for (ModuleNodeRc& input : inputs)
            input->connect(modules[1]);
        
    // remove the last module
    } else if (position == modules.size() - 1) {
        if (output != nullptr) modules[modules.size() - 2]->connect(output);
    
    // remove a module in the middle
    } else {
        ModuleNodeRc& prev = modules[position - 1];
        ModuleNodeRc& next = modules[position + 1];

        prev->connect(next);
    }

    // now, remove the module in the array
    modules.erase(modules.begin() + position);

    // move the reference to the caller
    return target_module;
}

bool EffectsRack::disconnect_input(ModuleNodeRc& input) {
    for (auto it = inputs.begin(); it != inputs.end(); it++)
    {
        if (*it == input)
        {
            inputs.erase(it);
            return true;
        }
    }

    return false;
}

ModuleNodeRc EffectsRack::disconnect_output()
{
    ModuleNodeRc& old_output = output;

    if (output != nullptr)
    {
        if (modules.empty())
        {
            for (ModuleNodeRc& input : inputs)
                input->disconnect();
        }
        else modules.back()->disconnect();
    }

    output = nullptr;
    return old_output;
}

void EffectsRack::connect_input(ModuleNodeRc& new_input) {
    if (modules.empty()) {
        if (output != nullptr)   
            new_input->connect(output);
    } else {
        new_input->connect(modules.front());
    }
    
    inputs.push_back(new_input);
}

ModuleNodeRc EffectsRack::connect_output(ModuleNodeRc& new_output) {
    ModuleNodeRc old_output = disconnect_output();
    output = new_output;

    if (output != nullptr) {
        if (modules.empty()) {
            for (ModuleNodeRc& input : inputs)
                input->connect(output);
            
        } else {
            modules.back()->connect(output);
        }
    }
    
    return old_output;
}

void EffectsRack::disconnect_all_inputs()
{
    for (ModuleNodeRc& input : inputs)
    {
        input->disconnect();
    }

    inputs.clear();
}















//////////////////////
//   EFFECTS BUS    //
//////////////////////
FXBus::FXBus(ModuleContext& modctx)
{
    controller = modctx.create<FaderModule>();
    strcpy(name, "FX Bus");
    rack.connect_output(controller);
}

FXBus::~FXBus()
{
    controller->remove_all_connections();
}

ModuleNodeRc FXBus::connect_output(ModuleNodeRc& output)
{
    ModuleNodeRc& old_output = controller->get_output();
    controller->disconnect();
    controller->connect(output);
    return old_output; 
}

ModuleNodeRc FXBus::disconnect_output()
{
    ModuleNodeRc old_output = controller->get_output();
    controller->disconnect();
    return old_output;
}

void FXBus::FaderModule::process(
    float** inputs,
    float* output,
    size_t num_inputs,
    size_t buffer_size,
    int sample_rate,
    int channel_count
)
{
    float smp[2];
    float accum[2];
    bool is_muted = mute || mute_override;

    float factor = powf(10.0f, gain / 10.0f);

    // clear buffer to zero
    for (size_t i = 0; i < buffer_size; i++)
    {
        output[i] = 0.0f;
    }

    for (size_t i = 0; i < buffer_size; i += 2)
    {
        smp[0] = 0.0f;
        smp[1] = 0.0f;

        for (size_t j = 0; j < num_inputs; j++)
        {
            smp[0] += inputs[j][i] * factor;
            smp[1] += inputs[j][i + 1] * factor;
        }

        if (!is_muted)
        {
            output[i] = smp[0];
            output[i + 1] = smp[1];
        }
        
        if (fabsf(smp[0]) > smp_accum[0]) smp_accum[0] = fabsf(smp[0]);
        if (fabsf(smp[1]) > smp_accum[1]) smp_accum[1] = fabsf(smp[1]);

        if (++smp_count > window_size)
        {
            analysis_volume[0] = smp_accum[0];
            analysis_volume[1] = smp_accum[1];

            smp_accum[0] = smp_accum[1] = 0.0f;
            smp_count = 0;
        }
    }
}