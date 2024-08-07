#include "song.hpp"
#include <climits>
#include <memory>
#include <vector>

using namespace sbox;

/////////////////////
// Notes, Patterns //
/////////////////////
static unsigned int new_note_id = 0;

Note::Note()
    : Note(0.0f, 0, 0.0f)
{}

Note::Note(float time, int key, float length) :
    time(time),
    key(key),
    length(length),
    id(new_note_id++)
{}

void Note::new_id()
{
    id = new_note_id++;
}

Note& Pattern::add_note(float time, int key, float length) {
    notes.push_back(Note(time, key, length));
    return *(notes.end() - 1);
}

////////////////
// ModuleRack //
////////////////

ModuleRack::ModuleRack()
{
    _in = nullptr;
    _out = nullptr;
}

static void connect(modx::ModuleRc &mod_a, modx::ModuleRc &mod_b)
{
    // connect first audio output to first audio input
    if (mod_a->audio_input_count() > 0 && (mod_b->audio_input_count() > 0 || mod_b->class_name() == modules::AudioEngine::MODULE_CLASS_STEREO_MIXER))
    {
        mod_a->connect_audio(*mod_b, 0, 0);
    }

    // connect first message output to first message input
    if (mod_a->message_input_count() > 0 && mod_b->message_input_count() > 0)
    {
        mod_b->connect_message(*mod_b, 0, 0);
    }
}

static void disconnect_output(modx::ModuleRc &mod)
{
    if (mod->audio_output_count() > 0) mod->disconnect_audio_output(0);
    if (mod->message_output_count() > 0) mod->disconnect_message_output(0);
}

static void disconnect_input(modx::ModuleRc &mod)
{
    if (mod->audio_input_count() > 0) mod->disconnect_audio_input(0);
    if (mod->message_input_count() > 0) mod->disconnect_message_input(0);
}

void ModuleRack::insert(modx::ModuleRc &module, size_t index)
{
    assert(index <= _modules.size());
    if (index > _modules.size())
    {
        logger::log_error("ModuleRack::insert out of bounds");
        return;
    }

    // add to front?
    if (index == 0)
    {
        if (_modules.size() > 0)
            connect(module, _modules.front());

        _modules.insert(_modules.begin(), module);

        if (_in)
            connect(_in, _modules.front());

        if (_modules.size() == 1 && _out)
            connect(module, _out);
    }

    // add to back?
    else if (index == _modules.size())
    {
        connect(_modules.back(), module);
        _modules.push_back(module);

        if (_out)
            connect(module, _out);
    }
    else
    {
        _modules.insert(_modules.begin(), module);
        connect(*(_modules.begin() + (index - 1)), module);
        connect(module, *(_modules.begin() + (index + 1)));
    }
}

modx::ModuleRc ModuleRack::remove(size_t index)
{
    assert(index < _modules.size());
    if (index >= _modules.size())
    {
        logger::log_error("ModuleRack::remove out of bounds");
        return modx::ModuleRc();
    }

    // remove from front?
    if (index == 0)
    {
        modx::ModuleRc mod = _modules.front();
        _modules.erase(_modules.begin());
        disconnect_input(mod);
        disconnect_output(mod);

        if (_modules.size() > 0 && _in)
            connect(_in, _modules.front());

        if (_modules.size() == 1 && _out)
            connect(_modules.back(), _out);

        return mod;
    }

    // remove from back?
    else if (index == _modules.size() - 1)
    {
        modx::ModuleRc mod = _modules.back();
        _modules.pop_back();
        disconnect_input(mod);
        disconnect_output(mod);

        if (_modules.size() > 0 && _out)
            connect(_modules.back(), _out);

        if (_modules.size() == 1 && _in)
            connect(_in, _modules.front());

        return mod;
    }

    else
    {
        modx::ModuleRc mod = _modules[index];
        disconnect_input(mod);
        disconnect_output(mod);

        connect(*(_modules.begin() + (index - 1)), *(_modules.begin() + (index + 1)));
        _modules.erase(_modules.begin() + index);

        return mod;
    }
}

void ModuleRack::connect_input(modx::ModuleRc &new_input)
{
    _in = new_input;

    if (_modules.size() > 0)
        connect(_in, _modules.front());
}

void ModuleRack::connect_output(modx::ModuleRc &new_output)
{
    _out = new_output;

    if (_modules.size() > 0)
        connect(_modules.back(), _out);
}



////////////////////
// Song, Channels //
////////////////////
std::unique_ptr<InstrumentChannel> Song::create_instrument_channel(modules::AudioEngine &engine, unsigned int index, unsigned int seq_length)
{
    std::unique_ptr<InstrumentChannel> channel = std::make_unique<InstrumentChannel>();
    channel->name = "Channel " + std::to_string(seq_length);
    channel->output_fader = modx::create_module(engine, "sbox::fader");
    channel->input_midi = modx::create_module(engine, "sbox::midi_in");
    channel->rack.connect_input(channel->input_midi);
    channel->rack.connect_output(channel->output_fader);

    channel->sequence.resize(seq_length);
    for (unsigned int j = 0; j < seq_length; j++)
        channel->sequence[j] = 0;

    channel->_effect_channel = (uint)-1;
    return channel;
}

std::unique_ptr<EffectChannel> Song::create_effect_channel(modules::AudioEngine &engine, unsigned int name_number)
{
    std::unique_ptr<EffectChannel> channel = std::make_unique<EffectChannel>();
    channel->name = name_number == 0 ? "Master" : ("Channel " + std::to_string(name_number));
    channel->input_mixer = modx::create_module(engine, modules::AudioEngine::MODULE_CLASS_STEREO_MIXER);
    channel->output_fader = modx::create_module(engine, "sbox::fader");
    channel->rack.connect_input(channel->input_mixer);
    channel->rack.connect_output(channel->output_fader);
    channel->_output_channel = (uint)-1;

    return channel;
}

Song::Song(unsigned int num_channels, unsigned int length, unsigned int max_patterns, modules::AudioEngine &audio_engine) :
    _audio_engine(audio_engine)
{
    _length = length;
    _max_patterns = max_patterns;
    name = "Unnamed";
    project_notes = "";
    tempo = 150.0f;
    beats_per_bar = 8;
    bar_position = 0;
    position = 0.0f;
    do_loop = true;

    _audio_out = modx::create_module(audio_engine, modules::AudioEngine::MODULE_CLASS_AUDIO_OUT);

    // create master effect channel
    {
        std::unique_ptr<EffectChannel> ch = create_effect_channel(_audio_engine, 0);
        connect(ch->output_fader, _audio_out);
        _fx_channels.push_back(std::move(ch));
    }

    // create instrument channels
    _channels.resize(num_channels);
    for (unsigned int i = 0; i < num_channels; i++)
    {
        _channels[i] = create_instrument_channel(_audio_engine, i, _length);
        route_instrument(i, 0);
    }
}

void Song::set_max_patterns(unsigned int count)
{
    assert(count > 0);
    _max_patterns = count;

    for (auto &ch : _channels)
    {
        if (ch->patterns.size() > _max_patterns)
        {
            ch->patterns.resize(_max_patterns);
        }
        
        for (auto it = ch->sequence.begin(); it != ch->sequence.end(); it++)
        {
            if (*it > _max_patterns) *it = 0;
        }
    }
}

unsigned int Song::new_pattern(unsigned int channel)
{
    assert(channel < _channels.size());
    std::unique_ptr<InstrumentChannel> &ch = _channels[channel];

    ch->patterns.push_back(std::make_unique<Pattern>());
    assert(ch->patterns.size() + 1 <= UINT_MAX);
    return (unsigned int) ch->patterns.size() + 1;
}

void Song::insert_bar(unsigned int bar_position)
{
    assert(bar_position <= _length);
    if (bar_position > _length) return;

    // add bar to end of song?
    if (bar_position == _length)
    {
        for (auto &ch : _channels)
        {
            assert(ch->sequence.size() == _length);
            ch->sequence.push_back(0);
        }
    }
    else
    {
        for (auto &ch : _channels)
        {
            assert(ch->sequence.size() == _length);
            ch->sequence.insert(ch->sequence.begin() + bar_position, 0);
        }
    }

    _length++;
}

void Song::remove_bar(unsigned int bar_position)
{
    assert(bar_position < _length);
    if (bar_position >= _length) return;

    for (auto &ch : _channels)
    {
        assert(ch->sequence.size() == _length);
        ch->sequence.erase(ch->sequence.begin() + bar_position);
    }

    _length--;
}

std::vector<unsigned int> Song::get_bar_patterns(unsigned int bar_position)
{
    assert(bar_position < _length);
    if (bar_position >= _length)
        return std::vector<unsigned int>();

    std::vector<unsigned int> ret;
    ret.resize(_channels.size());

    unsigned int i = 0;
    for (auto &ch : _channels)
    {
        assert(ch->sequence.size() == _length);
        ret[i] = ch->sequence[bar_position];
        i++;
    }

    return ret;
}

void Song::set_bar_patterns(unsigned int bar_position, unsigned int *array, size_t size)
{
    assert(bar_position < _length);
    if (bar_position >= _length)
    {
        logger::log_error("Song::set_bar_patterns position is out of range");
        return;
    }

    assert(size == _channels.size());
    if (size != _channels.size())
    {
        logger::log_error("Song::set_bar_patterns input size mismatch");
        return;
    }

    for (auto &ch : _channels)
    {
        assert(ch->sequence.size() == _length);
        ch->sequence[bar_position] = *array++;
    }
}

void Song::insert_channel(unsigned int index)
{
    assert(index <= _channels.size());
    if (index > _channels.size())
    {
        logger::log_error("Song::insert_channel: index out of range");
        return;
    }

    _channels.insert(_channels.begin() + index, create_instrument_channel(_audio_engine, index, _length));
}

void Song::remove_channel(unsigned int index)
{
    assert(index < _channels.size());
    if (index >= _channels.size())
    {
        logger::log_error("Song::remove_channel: index out of range");
        return;
    }

    // this should destroy all modules related to the channel, thus
    // disconnecting it from the connected mixer
    _channels.erase(_channels.begin() + index);
}

unsigned int Song::first_empty_pattern(unsigned int channel_index) const
{
    assert(channel_index < _channels.size());
    const std::unique_ptr<InstrumentChannel> &ch = _channels[channel_index];

    unsigned int i = 0;
    for (auto &p : ch->patterns)
    {
        if (p->is_empty()) return i;
        i++;
    }

    return 0;
}

void Song::insert_effect_channel(unsigned int index)
{
    assert(index <= _channels.size());
    if (index > _channels.size())
    {
        logger::log_error("Song::insert_effect_channel: index out of range");
        return;
    }

    if (index == 0)
    {
        logger::log_error("Song::insert_effect_channel: cannot modify index 0 (master channel)");
        return;
    }

    _fx_channels.insert(_fx_channels.begin() + index, create_effect_channel(_audio_engine, _fx_channels.size()));

    // update fx channel references
    for (unsigned int i = 0; i < _channels.size(); i++)
    {
        auto &inst_ch = _channels[i];

        if (inst_ch->effect_channel() >= index)
            route_instrument(i, inst_ch->effect_channel() + 1);
    }

    for (unsigned int i = 0; i < _fx_channels.size(); i++)
    {
        auto &fx_ch = _fx_channels[i];

        if (fx_ch->output_channel() != (uint)-1 && fx_ch->output_channel() >= index)
            route_effect(i, fx_ch->output_channel() + 1);
    }
}

void Song::remove_effect_channel(unsigned int index)
{
    assert(index < _channels.size());
    if (index >= _channels.size())
    {
        logger::log_error("Song::remove_effect_channel: index out of range");
        return;
    }

    if (index == 0)
    {
        logger::log_error("Song::remove_effect_channel: cannot modify index 0 (master channel)");
        return;
    }

    // this should destroy all modules related to the channel, thus
    // disconnecting all of its inputs
    _fx_channels.erase(_fx_channels.begin() + index);

    // update fx channel references
    for (unsigned int i = 0; i < _channels.size(); i++)
    {
        auto &inst_ch = _channels[i];
        if (inst_ch->effect_channel() == index)
            route_instrument(i, 0); // route to master
    }

    for (unsigned int i = 0; i < _fx_channels.size(); i++)
    {
        auto &fxch = _fx_channels[i];

        if (fxch->output_channel() == index)
            disconnect_effect(i);
    }
}

void Song::route_instrument(unsigned int channel_index, unsigned int effect_channel_index)
{
    assert(channel_index < _channels.size());
    assert(effect_channel_index < _fx_channels.size());

    if (channel_index >= _channels.size() || effect_channel_index >= _fx_channels.size())
    {
        logger::log_error("Song::route_instrument: channel_index or effect_channel_index is out of range");
        return;    
    }

    auto &inst_ch = _channels[channel_index];
    auto &fx_ch = _fx_channels[effect_channel_index];

    connect(inst_ch->output_fader, fx_ch->input_mixer);
    inst_ch->_effect_channel = effect_channel_index;
}

void Song::route_effect(unsigned int effect_channel_src_index, unsigned int effect_channel_dst_index)
{
    assert(effect_channel_src_index < _fx_channels.size());
    assert(effect_channel_dst_index < _fx_channels.size());

    if (effect_channel_src_index >= _fx_channels.size() || effect_channel_dst_index >= _fx_channels.size())
    {
        logger::log_error("Song::route_effect: effect_channel_src_index or effect_channel_dst_index is out of range");
        return;
    }

    auto &fx_src = _fx_channels[effect_channel_src_index];
    auto &fx_dst = _fx_channels[effect_channel_dst_index];

    connect(fx_src->output_fader, fx_dst->input_mixer);
    fx_src->_output_channel = effect_channel_dst_index;
}

void Song::disconnect_effect(unsigned int channel_index)
{
    assert(channel_index < _fx_channels.size());
    if (channel_index >= _fx_channels.size())
    {
        logger::log_error("Song::disconnect_effect: index is out of range");
        return;
    }

    auto &fx = _fx_channels[channel_index];
    disconnect_output(fx->output_fader);
    fx->_output_channel = (uint)-1;
}