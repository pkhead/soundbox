#pragma once
#include "audio_engine/audio_engine.hpp"
#include <module_hosts/modules.hpp>
#include <app/song.hpp>

namespace hosts::internal
{
    /**
    * ID: sbox::channel_controller
    * Processes the track of an instrument channel to send it to its output message port.
    * Can also receive track messages sent from the main thread through its "midi_queue" internal buffer.
    * This is used to play notes from the user's MIDI keyboard. 
    **/
    class ChannelControllerModule : public modx::ModuleBase
    {
    private:
        // the note data of a specific channel
        struct TrackInfo
        {
            std::vector<unsigned int> sequence;
            std::vector<sbox::Pattern> patterns;
        };

        enum InputMessageType : uint8_t
        {
            MESSAGE_NEW_DATA,
            MESSAGE_SET_POSITION,
            MESSAGE_SET_PLAYBACK_INFO,

            MESSAGE_PLAY,
            MESSAGE_STOP,
            MESSAGE_SEND_TRACK_INFO,

            /**
            * Expects a track event afterward.
            **/
            MESSAGE_TRACK_EVENT
        };

        enum OutputMessageType
        {
            /**
            * Expects a TrackInfo* afterward.
            **/
            MESSAGE_DISCARD_DATA
        };

        struct InputMessage
        {
            InputMessageType type;

            union
            {
                TrackInfo *new_data;
                double position;
                modx::TrackEvent track_event;

                struct
                {
                    float tempo;
                    uint8_t beats_per_bar;
                } playback_info;
            };
        };

        struct OutputMessage
        {
            OutputMessageType type;

            union
            {
                TrackInfo *discard_data;
            };
        };

        /// This is written to by the main thread and read by the audio process thread.
        RingBuffer<InputMessage> input_queue;

        /// This is written to by the audio thread and read by the main thread.
        RingBuffer<OutputMessage> output_queue;

        static constexpr size_t MAX_ACTIVE_NOTES = 64;
        sbox::Note cur_active_notes[MAX_ACTIVE_NOTES];
        unsigned int frames_unprocessed;

        TrackInfo *_track;
        double _position;
        float _tempo;
        uint8_t _beats_per_bar;
        bool _is_playing;
        bool _was_playing;

        sbox::Note *_old_active_notes;
        unsigned int _old_active_note_count;

        void read_events(modules::ModuleProcessor &proc);

    public:
        ChannelControllerModule(modules::ModuleCreator &create);
        ~ChannelControllerModule();

        void process(modules::ModuleProcessor &proc) override;
        void idle();

        void set_track(sbox::InstrumentChannel &channel);
        void set_position(double position);
        void set_playback_info(float tempo, uint8_t beats_per_bar);
        void set_playing(bool play_state);
        void force_send_track_info();
        void send_event(const modx::TrackEvent &event);
    }; // class ChannelControllerModule
} // namespace hosts::internal