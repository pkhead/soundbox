#include "Tunings.h"
#include "audio.h"
#include "modules/modules.h"
#include "sys.h"
#include "song.h"
#include "editor/editor.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sstream>

/*
Song data structure (OLD):

struct note {
    float time;
    int16_t key;
    float length;
}

struct pattern {
    uint32_t num_notes;
    struct note notes[num_notes];
}

struct channel {
    uint8_t name_size;
    char name[name_size];

    float volume, panning;
    uint8_t inst_id_size;
    char inst_id[inst_id_size];
    uint64_t inst_state_size;
    uint8_t inst_state[inst_state_size];

    uint32_t sequence[song::length];
    struct pattern patterns[song::max_patterns];
}

struct song {
    uint8_t name_size;
    char name[name_size];

    uint32_t length;
    uint32_t num_channels;
    uint32_t max_patterns;
    uint32_t beats_per_bar;
    float tempo;

    struct channel channels[num_channels];
}

struct file {
    char magic[4] = "SnBx";
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t version_revision;

    struct song song_data;
}
*/

static void save_module(std::ostream& out, audiomod::ModuleBase& mod)
{
    std::stringstream stream;

    // store module type
    push_bytes(out, (uint8_t) strlen(mod.id));
    out << mod.id;

    // store module state
    mod.save_state(stream);
    size_t state_size = stream.tellp();
    push_bytes<uint64_t>(out, state_size);
    if (state_size > 0) out << stream.rdbuf();
}

static audiomod::ModuleNodeRc load_module(
    std::istream& input, Song* song,
    audiomod::ModuleContext& modctx,
    plugins::PluginManager& plugin_manager,
    WorkScheduler& work_scheduler,
    std::string* error_msg
) {
    // read mod type
    uint8_t id_size = pull_bytesr<uint8_t>(input);
    char* inst_id = new char[id_size + 1];
    input.read(inst_id, id_size);
    inst_id[id_size] = 0;

    // load module based off id
    audiomod::ModuleNodeRc mod = audiomod::create_module(inst_id, modctx, plugin_manager, work_scheduler);
    if (mod == nullptr) {
        if (error_msg != nullptr) *error_msg = "unknown module type " + std::string(inst_id);
        delete[] inst_id;
        return nullptr;
    }
    mod->module().song = song;

    // load state
    uint64_t mod_state_size = pull_bytesr<uint64_t>(input);

    if (mod_state_size > 0)
    {
        // copy N bytes of input stream to temp stream
        // N = mod_state_size
        std::stringstream temp;
        char* buf = new char[mod_state_size];
        input.read(buf, mod_state_size);
        temp.write(buf, mod_state_size);

        mod->module().load_state(temp, mod_state_size);

        delete[] buf;
    }

    delete[] inst_id;
    return mod;
}

void Song::serialize(std::ostream& out) const {
    // magic number
    out << "SnBx";

    // file version number
    push_bytes<uint32_t>(out, 1);
    
    // song name
    push_bytes(out, (uint8_t) strlen(name));
    out << name;

    // v5: project notes
    push_bytes(out, (uint32_t)project_notes.size());
    out << project_notes;

    // write song properties
    push_bytes(out, (uint32_t) length());
    push_bytes(out, (uint32_t) channels.size());
    push_bytes(out, (uint32_t) max_patterns());
    push_bytes(out, (uint32_t) beats_per_bar);
    push_bytes(out, (float) tempo);

    // v3: tuning data
    // first entry (12edo) is ignored, so subtract size by 1
    push_bytes(out, (uint8_t) (tunings.size() - 1));
    for (Tuning* tuning : tunings)
    {
        // ignore first entry (12edo)
        if (!tuning->is_12edo)
        {
            // write name
            push_bytes(out, (uint32_t) tuning->name.size());
            out << tuning->name;
            
            // write description
            push_bytes(out, (uint32_t) tuning->desc.size());
            out << tuning->desc;

            // write key frequencies
            push_bytes(out, (uint32_t) tuning->key_freqs.size());
            for (float freq : tuning->key_freqs)
            {
                push_bytes(out, (float) freq);
            }

            // v6: write scala data
            if (tuning->scl_import == nullptr)
            {
                push_bytes(out, (uint32_t) 0);
                push_bytes(out, (uint32_t) 0);
            }
            else
            {
                TuningSclImport& scl_import = *tuning->scl_import;
                
                // write scl data
                push_bytes(out, (uint32_t) scl_import.scl.rawText.size());
                out << scl_import.scl.rawText;

                // write kbm data
                push_bytes(out, (uint32_t) scl_import.kbm.rawText.size());
                out << scl_import.kbm.rawText;
            }
        }
    }

    // write the selected tuning
    push_bytes(out, (uint8_t) selected_tuning);

    // v4: fx mixer
    push_bytes(out, (uint16_t) fx_mixer.size());
    for (auto& bus : fx_mixer)
    {
        // write bus name
        push_bytes(out, (uint8_t) strlen(bus->name));
        out << bus->name;

        // write mute/solo
        uint8_t fx_state = 0;
        if (dynamic_cast<audiomod::FXBus::FaderModule&>(bus->controller->module()).mute)   fx_state |= 1;
        if (bus->solo)              fx_state |= 2;
        push_bytes(out, (uint8_t) fx_state);

        // write output bus
        if (bus != fx_mixer[0])
            push_bytes(out, (uint16_t) bus->target_bus);

        // write effects
        push_bytes(out, (uint8_t) bus->get_modules().size());
        for (audiomod::ModuleNodeRc& mod : bus->get_modules())
            save_module(out, mod->module());
    }

    // write the channel data
    for (auto& channel : channels) {
        push_bytes(out, (uint8_t) strlen(channel->name));
        out << channel->name;

        audiomod::VolumeModule& vol_mod = channel->vol_mod->module<audiomod::VolumeModule>();
        push_bytes(out, (float) vol_mod.volume);
        push_bytes(out, (float) vol_mod.panning);

        // write channel flags (mute, solo)
        uint8_t channel_flags = 0;
        if (vol_mod.mute)  channel_flags |= 1;
        if (channel->solo)          channel_flags |= 2;

        push_bytes(out, (uint8_t) channel_flags);

        // v4: write output fx bus
        push_bytes(out, (uint16_t) channel->fx_target_idx);

        // save channel instrument
        save_module(out, channel->synth_mod->module());
        
        // v4: store effects
        push_bytes(out, (uint8_t) channel->effects_rack.modules.size());

        for (audiomod::ModuleNodeRc& mod : channel->effects_rack.modules)
            save_module(out, mod->module());

        // write channel sequence
        for (int& id : channel->sequence) {
            push_bytes(out, (uint32_t) id);
        }

        // write pattern data
        for (auto& pattern : channel->patterns) {
            push_bytes(out, (uint32_t) pattern->notes.size());

            for (Note& note : pattern->notes) {
                push_bytes(out, (float) note.time);
                push_bytes(out, (int16_t) note.key);
                push_bytes(out, (float) note.length);
            }
        }
    }
}

std::unique_ptr<Song> Song::from_file(
    std::istream& input,
    audiomod::ModuleContext& modctx,
    plugins::PluginManager& plugin_manager,
    std::string* error_msg
) {
    // check if the magic number is valid
    char magic_number[4];
    input.read(magic_number, 4);
    if (memcmp("SnBx", magic_number, 4) != 0) return nullptr;

    uint32_t version = pull_bytesr<uint32_t>(input);
    if (version != 1)
    {
        if (error_msg) *error_msg = "invalid version";
        return nullptr;
    }
    
    // get song name
    uint8_t song_name_size;
    pull_bytes(input, song_name_size);

    char* song_name = new char[song_name_size + 1];
    input.read(song_name, song_name_size);
    song_name[song_name_size] = 0;

    // read song notes
    std::string project_notes = "";

    {
        uint32_t project_notes_len;

        pull_bytes(input, project_notes_len);

        if (project_notes_len > 0)
        {
            project_notes.resize(project_notes_len);
            input.read(&project_notes.front(), project_notes_len);
        }
    }

    // get song data
    uint32_t length, num_channels, max_patterns, beats_per_bar;
    pull_bytes(input, length);
    pull_bytes(input, num_channels);
    pull_bytes(input, max_patterns);
    pull_bytes(input, beats_per_bar);

    float tempo;
    pull_bytes(input, tempo);

    // create song
    std::unique_ptr<Song> song = std::make_unique<Song>(num_channels, length, max_patterns, modctx);
    strncpy(song->name, song_name, song->name_capcity);
    song->project_notes = project_notes;
    song->name[song_name_size] = 0;
    song->beats_per_bar = beats_per_bar;
    song->tempo = tempo;

    WorkScheduler& work_scheduler = song->work_scheduler;

    delete[] song_name;

    // tuning data
    {
        uint8_t num_tunings;
        pull_bytes(input, num_tunings);

        // read tunings
        for (uint8_t i = 0; i < num_tunings; i++)
        {
            Tuning* tuning = new Tuning();
            uint32_t name_size, desc_size, keys_size;
            float freq;

            // read name
            pull_bytes(input, name_size);

            if (name_size > 0)
            {
                tuning->name.resize(name_size);
                input.read(&tuning->name.front(), name_size);
            }
            else tuning->name.clear();

            // read description
            pull_bytes(input, desc_size);
            
            if (desc_size > 0)
            {
                tuning->desc.resize(desc_size);
                input.read(&tuning->desc.front(), desc_size);
            }
            else tuning->desc.clear();

            // get key frequencies
            pull_bytes(input, keys_size);
            for (uint32_t i = 0; i < keys_size; i++)
            {
                pull_bytes(input, freq);
                tuning->key_freqs.push_back(freq);
            }

            // read scala data
            {
                uint32_t scl_size, kbm_size;
                pull_bytes(input, scl_size);

                if (scl_size > 0)
                {
                    std::string scl_data, kbm_data;

                    // read scl
                    scl_data.resize(scl_size);
                    input.read(&scl_data.front(), scl_size);

                    // read kbm
                    pull_bytes(input, kbm_size);
                    kbm_data.resize(kbm_size);
                    input.read(&kbm_data.front(), kbm_size);

                    TuningSclImport* scl_import = new TuningSclImport;
                    tuning->scl_import = scl_import;

                    try {
                        scl_import->scl = Tunings::parseSCLData(scl_data);
                        scl_import->kbm = Tunings::parseKBMData(kbm_data); 
                    } catch(Tunings::TuningError& err) {
                        if (error_msg) *error_msg = err.what();

                        delete tuning;
                        delete scl_import;
                        return nullptr;
                    }
                }
                else // no scala data
                {
                    pull_bytes(input, kbm_size);
                    if (kbm_size != 0) {
                        delete tuning;
                        return nullptr;
                    }
                }
            }

            tuning->analyze();
            song->tunings.push_back(tuning);
        }

        // selected tuning
        uint8_t selected_tuning_uint8;
        pull_bytes(input, selected_tuning_uint8);
        song->selected_tuning = selected_tuning_uint8;
    }

    // fx mixer
    {
        uint16_t num_buses;
        pull_bytes(input, num_buses);

        for (uint16_t i = 0; i < num_buses; i++)
        {
            audiomod::FXBus* bus;
            
            if (i == 0)
                bus = song->fx_mixer[0].get();
            else {
                bus = new audiomod::FXBus(modctx);
                song->fx_mixer.push_back(std::unique_ptr<audiomod::FXBus>(bus));
            }

            // get bus name
            uint8_t name_size = pull_bytesr<uint8_t>(input);
            
            if (name_size > bus->name_capacity) {
                if (error_msg) *error_msg = "name length exceeds capacity";
                return nullptr;
            }

            input.read(bus->name, name_size);
            bus->name[name_size] = 0;

            // get mute/solo
            uint8_t flags;
            pull_bytes(input, flags);

            if ((flags & 1) == 1) dynamic_cast<audiomod::FXBus::FaderModule&>(bus->controller->module()).mute = true; // mute
            if ((flags & 2) == 2) bus->solo = true;            // solo

            // get output bus
            if (i > 0) {
                uint16_t target_bus;
                pull_bytes(input, target_bus);
                bus->target_bus = target_bus;
            }

            // get effects
            uint8_t mod_count;
            char inst_id[256];
            pull_bytes(input, mod_count);

            for (uint8_t j = 0; j < mod_count; j++)
            {
                // read mod type
                audiomod::ModuleNodeRc mod = load_module(input, song.get(), modctx, plugin_manager, work_scheduler, error_msg);
                if (mod == nullptr) {
                    return nullptr;
                }

                mod->module().parent_name = bus->name;
                bus->insert(mod);
            }
        }

        // connect the fx buses
        for (auto& bus : song->fx_mixer)
        {
            if (bus == song->fx_mixer.front()) continue;
            song->fx_mixer[bus->target_bus]->connect_input(bus->controller);
        }
    }

    // retrive channel data
    for (uint32_t channel_i = 0; channel_i < num_channels; channel_i++) {
        auto& channel = song->channels[channel_i];

        // channel name
        uint8_t channel_name_size;
        pull_bytes(input, channel_name_size);

        input.read(channel->name, channel_name_size); // TODO: cap to channel name capacity (16 bytes)
        channel->name[channel_name_size] = 0;

        // volume, panning
        float volume, panning;

        pull_bytes(input, volume);
        pull_bytes(input, panning);

        audiomod::VolumeModule& vol_mod = channel->vol_mod->module<audiomod::VolumeModule>();
        vol_mod.volume = volume;
        vol_mod.panning = panning;

        // mute/solo flags
        {
            uint8_t channel_flags;
            pull_bytes(input, channel_flags);

            vol_mod.mute =          (channel_flags & 1) == 1;
            channel->solo =         (channel_flags & 2) == 2;
        }

        // output fx bus
        {
            uint16_t output_bus;
            pull_bytes(input, output_bus);
            channel->fx_target_idx = output_bus;
        }

        // instrument data
        {
            auto mod = load_module(input, song.get(), modctx, plugin_manager, work_scheduler, error_msg);
            if (mod == nullptr) {
                return nullptr;
            }

            mod->module().parent_name = channel->name;
            channel->set_instrument(mod);
        }

        // instrument effects
        {
            uint8_t num_mods;
            pull_bytes(input, num_mods);

            for (uint8_t modi = 0; modi < num_mods; modi++)
            {
                audiomod::ModuleNodeRc mod = load_module(input, song.get(), modctx, plugin_manager, work_scheduler, error_msg);
                if (mod == nullptr) {
                    return nullptr;
                }

                mod->module().parent_name = channel->name;
                channel->effects_rack.insert(mod);
            }

            // connect channel to target fx bus
            song->fx_mixer.front()->disconnect_input(channel->vol_mod);
            song->fx_mixer[channel->fx_target_idx]->connect_input(channel->vol_mod);
        }

        // get sequence
        uint32_t pattern_id;
        for (uint32_t i = 0; i < length; i++) {
            pull_bytes(input, pattern_id);
            channel->sequence[i] = pattern_id;
        }

        // get pattern and note data
        for (uint32_t pattern_i = 0; pattern_i < max_patterns; pattern_i++) {
            uint32_t num_notes;
            pull_bytes(input, num_notes);

            for (uint32_t i = 0; i < num_notes; i++) {
                float time, length;
                int16_t key;
                
                pull_bytes(input, time);
                pull_bytes(input, key);
                pull_bytes(input, length);

                channel->patterns[pattern_i]->notes.push_back(Note(
                    time, key, length
                ));
            }
        }
    }

    return song;
}