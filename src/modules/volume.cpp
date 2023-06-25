#include "volume.h"
#include "../sys.h"

using namespace audiomod;

inline int sign(float v) {
    return v >= 0.0f ? 1 : -1; 
}

inline bool is_zero_crossing(float prev, float next) {
    return (prev == 0.0f && next == 0.0f) || (sign(prev) != sign(next));
}

VolumeModule::VolumeModule() : ModuleBase(false) {
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

void VolumeModule::save_state(std::ostream& ostream) const {
    push_bytes<uint8_t>(ostream, 0); // write version

    push_bytes<float>(ostream, volume);
    push_bytes<float>(ostream, panning);
    push_bytes<uint8_t>(ostream, mute);
}

bool VolumeModule::load_state(std::istream& istream, size_t size)
{
    uint8_t version = pull_bytesr<uint8_t>(istream);
    if (version != 0) return false;
    
    volume = pull_bytesr<float>(istream);
    panning = pull_bytesr<float>(istream);
    mute = pull_bytesr<uint8_t>(istream);
    
    return true;
}