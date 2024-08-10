#include <cassert>
#include <cmath>
#include <app/song.hpp>
#include <module_hosts/modules.hpp>
#include <audio_engine/audio_engine.hpp>
#include "channel_control.hpp"

using namespace hosts::internal;

constexpr size_t QUEUE_SIZE = 64;

ChannelControllerModule::ChannelControllerModule(modules::ModuleCreator &create) :
    modx::ModuleBase(create),
    input_queue(QUEUE_SIZE),
    output_queue(QUEUE_SIZE)
{
    create.add_message_output();
    _track = nullptr;
    _position = 0.0f;
    _tempo = 120.0f;
    _is_playing = false;
    _was_playing = false;

    _old_active_notes = new sbox::Note[MAX_ACTIVE_NOTES];
    _old_active_note_count = 0;
}

ChannelControllerModule::~ChannelControllerModule()
{
    delete[] _old_active_notes;
    delete _track;
}

void ChannelControllerModule::read_events(modules::ModuleProcessor &proc)
{
    // read input messages from internal buffer
    while (true)
    {
        InputMessage in_msg;
        if (!input_queue.read(&in_msg, 1)) break;

        switch (in_msg.type)
        {
            case MESSAGE_NEW_DATA:
            {
                // request main thread to free track info
                if (_track != nullptr)
                {
                    OutputMessage out_msg;
                    out_msg.type = MESSAGE_DISCARD_DATA;
                    out_msg.discard_data = _track;
                    output_queue.write(&out_msg, 1);
                }

                _track = in_msg.new_data;
                break;
            }
            
            case MESSAGE_SET_POSITION:
            {
                _position = in_msg.position;
                break;
            }
            
            case MESSAGE_SET_PLAYBACK_INFO:
            {
                _tempo = in_msg.playback_info.tempo;
                _beats_per_bar = in_msg.playback_info.beats_per_bar;

                modx::TrackEvent ev = modx::TrackEvent::init_tempo(_tempo);
                proc.send_message(0, &ev, sizeof(ev));
                break;
            }
            
            case MESSAGE_PLAY:
            {
                _is_playing = true;
                break;
            }
            
            case MESSAGE_STOP:
            {
                _is_playing = false;
                break;
            }
            
            case MESSAGE_TRACK_EVENT:
            {
                proc.send_message(0, &in_msg.track_event, sizeof(in_msg.track_event));
                break;
            }

            default: assert(false); // unreachable code
        }
    }
}

void ChannelControllerModule::process(modules::ModuleProcessor &proc)
{
    read_events(proc);

    // when stopped, release all active notes
    if (_was_playing != _is_playing)
    {
        if (!_is_playing)
        {
            for (unsigned int i = 0; i < _old_active_note_count; i++)
            {
                auto &active_note = _old_active_notes[i];

                modx::TrackEvent ev = modx::TrackEvent::init_note_off(active_note.key, 1.0f);
                ev.timestamp = 0;
                proc.send_message(0, &ev, sizeof(ev));
            }

            _old_active_note_count = 0;
        }
        
        _was_playing = _is_playing;
    }

    if (!_is_playing) return;
    if (_track == nullptr) return;

    float sample_len = 1.0f / proc.sample_rate;
    for (unsigned int frame = 0; frame < proc.buffer_frame_count; frame++)
    {
        // get the list of active notes
        sbox::Note cur_active_notes[MAX_ACTIVE_NOTES];
        unsigned int cur_active_note_count = 0;
        
        assert(_position >= 0.0f && _position < _track->sequence.size() * _beats_per_bar);
        float playhead_in_bar = fmodf(_position, _beats_per_bar);

        unsigned int pattern_index = _track->sequence[(int)(_position / _beats_per_bar)];
        if (pattern_index > 0)
        {
            auto &pattern = _track->patterns[pattern_index - 1];

            for (auto &note : pattern.notes)
            {
                const float note_start = note.time;
                const float note_end = note_start + note.length;

                if (playhead_in_bar >= note_start && playhead_in_bar < note_end)
                {
                    cur_active_notes[cur_active_note_count++] = note;
                    if (cur_active_note_count >= MAX_ACTIVE_NOTES) break;
                }
            }
        }

        // send released notes
        for (unsigned int i = 0; i < _old_active_note_count; i++)
        {
            auto &old_note = _old_active_notes[i];

            bool is_released = true;
            for (unsigned int j = 0; j < cur_active_note_count; j++)
            {
                auto &new_note = cur_active_notes[j];
                if (old_note.id == new_note.id)
                {
                    is_released = false;
                    break;
                }
            }

            if (!is_released) continue;

            modx::TrackEvent ev = modx::TrackEvent::init_note_off(old_note.key, 1.0f);
            ev.timestamp = frame;
            proc.send_message(0, &ev, sizeof(ev));
        }

        // send pressed notes
        for (unsigned int i = 0; i < cur_active_note_count; i++)
        {
            auto &new_note = cur_active_notes[i];
            bool is_pressed = true;

            for (unsigned int j = 0; j < _old_active_note_count; j++)
            {
                auto &old_note = _old_active_notes[j];
                if (new_note.id == old_note.id)
                {
                    is_pressed = false;
                    break;
                }
            }

            if (!is_pressed) continue;

            modx::TrackEvent ev = modx::TrackEvent::init_note_on(new_note.key, 1.0f);
            ev.timestamp = frame;
            proc.send_message(0, &ev, sizeof(ev));
        }

        memcpy(_old_active_notes, cur_active_notes, MAX_ACTIVE_NOTES * sizeof(sbox::Note));
        _old_active_note_count = cur_active_note_count;

        _position += (_tempo / 60.0f) * sample_len;
        _position = fmod(_position, _track->sequence.size() * _beats_per_bar);
    }
}

void ChannelControllerModule::idle()
{
    // process output messages
    while (true)
    {
        OutputMessage out_msg;
        if (!output_queue.read(&out_msg, 1)) break;
        
        switch (out_msg.type)
        {
            case MESSAGE_DISCARD_DATA:
            {
                delete out_msg.discard_data;
                break;
            }

            default: assert(false); // unreachable code
        }
    }
}

// control functions //
void ChannelControllerModule::set_track(sbox::InstrumentChannel &channel)
{
    TrackInfo *track_info = new TrackInfo;;
    track_info->sequence = channel.sequence;

    track_info->patterns.resize(channel.patterns.size());
    for (unsigned int i = 0; i < channel.patterns.size(); i++)
    {
        track_info->patterns[i] = *channel.patterns[i];
    }

    InputMessage msg{};
    msg.type = MESSAGE_NEW_DATA;
    msg.new_data = track_info;
    input_queue.write(&msg, 1);
}

void ChannelControllerModule::set_position(double position)
{
    InputMessage msg{};
    msg.type = MESSAGE_SET_POSITION;
    msg.position = position;
    input_queue.write(&msg, 1);
}

void ChannelControllerModule::set_playback_info(float tempo, uint8_t beats_per_bar)
{
    InputMessage msg{};
    msg.type = MESSAGE_SET_PLAYBACK_INFO;
    msg.playback_info.tempo = tempo;
    msg.playback_info.beats_per_bar = beats_per_bar;
    input_queue.write(&msg, 1);
}

void ChannelControllerModule::set_playing(bool play_state)
{
    InputMessage msg{};
    msg.type = play_state ? MESSAGE_PLAY : MESSAGE_STOP;
    input_queue.write(&msg, 1);

}
void ChannelControllerModule::send_event(const modx::TrackEvent &event)
{
    InputMessage msg{};
    msg.type = MESSAGE_TRACK_EVENT;
    msg.track_event = event;
    input_queue.write(&msg, 1);
}