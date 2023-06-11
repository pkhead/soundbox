#include "volume.h"
#include "sys.h"

using namespace audiomod;

inline int sign(float v) {
    return v >= 0.0f ? 1 : -1; 
}

inline bool is_zero_crossing(float prev, float next) {
    return (prev == 0.0f && next == 0.0f) || (sign(prev) != sign(next));
}

VolumeModule::VolumeModule(Song* song) : ModuleBase(song, false) {
    id = "effect.volume";
    cur_volume[0] = volume;
    cur_volume[1] = volume;
    last_sample[0] = 0.0f;
    last_sample[1] = 0.0f;
}

void VolumeModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    float r_mult = (panning + 1.0f) / 2.0f;
    float l_mult = 1.0f - r_mult;

    for (size_t i = 0; i < buffer_size; i += channel_count) {
        // set both channels to zero
        output[i] = 0.0f;
        output[i + 1] = 0.0f;

        if (!mute && !mute_override) {
            for (size_t j = 0; j < num_inputs; j++) {
                if (is_zero_crossing(last_sample[0], inputs[j][i])) cur_volume[0] = volume * l_mult;
                if (is_zero_crossing(last_sample[1], inputs[j][i + 1])) cur_volume[1] = volume * r_mult;

                output[i] += inputs[j][i] * cur_volume[0];
                output[i + 1] += inputs[j][i + 1] * cur_volume[1];


                last_sample[0] = output[i];
                last_sample[1] = output[i + 1];
            }
        }
    }
}

struct VolumeModuleState {
    float volume, panning;
    uint8_t mute;
};

size_t VolumeModule::save_state(void** output) const {
    VolumeModuleState* state = new VolumeModuleState;

    state->volume = swap_little_endian(volume);
    state->panning = swap_little_endian(panning);
    state->mute = mute;

    *output = state;
    return sizeof(VolumeModuleState);
}

bool VolumeModule::load_state(void* state_ptr, size_t size)
{
    if (size != sizeof(VolumeModuleState)) return false;
    
    VolumeModuleState* state = (VolumeModuleState*)state_ptr;
    
    volume = swap_little_endian(state->volume);
    panning = swap_little_endian(state->panning);
    mute = state->mute;
    
    return true;
}