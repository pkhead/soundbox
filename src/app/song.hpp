#pragma once
#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include "../modules/modules.hpp"
#include "../modules/internal/midi.hpp"
#include "audio_engine/audio_engine.hpp"

namespace sbox
{
    struct Note {
        float time;
        int key;
        float length;

        // Notes are not dynamically allocated so
        // we can't use their pointers for identification
        // this is why we make a unique id for all new notes
        unsigned int id;

        Note();
        Note(float time, int key, float length);

        void new_id();

        inline bool operator==(const Note& other) const noexcept {
            return id == other.id;
        }

        inline bool operator!=(const Note& other) const noexcept {
            return !(*this == other);
        }
    }; // struct Note

    struct Pattern {
        Pattern() {};

        std::vector<Note> notes;
        Note& add_note(float time, int key, float length);
        inline bool is_empty() const { return notes.empty(); }
    }; // struct Pattern

    class ModuleRack
    {
    private:
        std::vector<modx::ModuleRc> _modules;
        modx::ModuleRc _in;
        modx::ModuleRc _out;

    public:
        ModuleRack();

        inline size_t size() const {
            return _modules.size();
        }

        inline modx::ModuleRc& at(size_t index) {
            assert(index >= 0 && index < _modules.size());
            return _modules[index];
        }
        
        void insert(const modx::ModuleRc& module, size_t index);
        void insert(const modx::ModuleRc &module) { insert(module, size()); }
        modx::ModuleRc remove(size_t index);

        void connect_input(const modx::ModuleRc &new_input);
        inline const modx::ModuleRc& input() const {
            return _in;
        }

        void connect_output(const modx::ModuleRc &new_output);
        inline const modx::ModuleRc& output() const {
            return _out;
        }
    }; // class ModuleRack

    class Song;

    class InstrumentChannel
    {
    private:
        unsigned int _effect_channel;
    
    public:
        std::string name;
        std::vector<unsigned int> sequence;
        std::vector<std::unique_ptr<Pattern>> patterns;

        modx::ModuleRc input_midi;
        modx::ModuleRc output_fader;
        ModuleRack rack;

        bool mute, solo;

        inline unsigned int effect_channel() const { return _effect_channel; }

        void send_midi(const midi::MidiEvent &event);

        friend class Song;
    }; // struct InstrumentChannel

    class EffectChannel
    {
    private:
        unsigned int _output_channel;
    
    public:
        std::string name;

        modx::ModuleRc input_mixer;
        modx::ModuleRc output_fader;
        ModuleRack rack;

        bool mute, solo;

        /**
        * Get the effect channel this channel is routed to.
        * @returns `(unsigned int)-1` if not routed to anything, otherwise the index of the output channel.
        **/
        inline unsigned int output_channel() const { return _output_channel; }

        friend class Song;
    }; // struct EffectChannel

    class Song
    {
    private:
        std::vector<std::unique_ptr<InstrumentChannel>> _channels;
        std::vector<std::unique_ptr<EffectChannel>> _fx_channels;

        unsigned int _length;
        unsigned int _max_patterns;
        modules::AudioEngine &_audio_engine;

        modx::ModuleRc _audio_out;

        std::unique_ptr<InstrumentChannel> create_instrument_channel(modules::AudioEngine &engine, unsigned int index);
        static std::unique_ptr<EffectChannel> create_effect_channel(modules::AudioEngine &engine, unsigned int name_number);

    public:
        Song(const Song&) = delete; // disable copy
        Song(unsigned int num_channels, unsigned int length, unsigned int max_patterns, modules::AudioEngine &audio_engine);
        inline modules::AudioEngine &audio_engine() { return _audio_engine; }

        std::string name;
        std::string project_notes;
        float tempo;
        unsigned int beats_per_bar;
        unsigned int bar_position;
        float position;
        bool do_loop;

        bool is_playing;

        inline unsigned int length() const { return _length; }
        inline unsigned int max_patterns() const { return _max_patterns; }
        void set_max_patterns(unsigned int count);

        /**
        * Create a new empty pattern for the given channel.
        * @param channel The channel index.
        * @returns The number of the new pattern.
        **/
        unsigned int new_pattern(unsigned int channel);

        void insert_bar(unsigned int bar_position);
        void remove_bar(unsigned int bar_position);
        std::vector<unsigned int> get_bar_patterns(unsigned int bar_position);
        void set_bar_patterns(unsigned int bar_position, unsigned int *array, size_t size);
        inline void set_bar_patterns(unsigned int bar_position, std::vector<unsigned int> patterns) {
            set_bar_patterns(bar_position, patterns.data(), patterns.size());
        }

        /**
        * Insert a new instrument channel.
        * @param index The index to insert the new channel to.
        **/
        void insert_channel(unsigned int index);

        /**
        * Remove an instrument channel.
        * @param index The channel index to remove.
        **/
        void remove_channel(unsigned int index);

        /**
        * Return the number of instrument channels.
        **/
        inline size_t channel_count() const {
            return _channels.size();
        };

        /**
        * Find the index of the first empty pattern in a channel index.
        * @param channel_index The index to the channel containing the patterns to search.
        * @returns The index of the first empty pattern. A value of 0 means there are no empty patterns in the channel.
        **/
        unsigned int first_empty_pattern(unsigned int chnanel_index) const;

        /**
        * Obtain a reference to the channel structure.
        **/
        inline InstrumentChannel& get_channel(unsigned int index) {
            assert(index < _channels.size());
            return *_channels[index];
        }

        /**
        * Return the number of effect channels.
        **/
        inline size_t effect_channel_count() const {
            return _fx_channels.size();
        }

        /**
        * Insert a new effect channel.
        * @param index The index to insert the new channel to. Must not be 0, as 0 is always the master channel.
        **/
        void insert_effect_channel(unsigned int index);

        /**
        * Remove an effect channel.
        * @param index The index of the channel to remove. Must not be 0, as 0 is always the master channel.
        **/
        void remove_effect_channel(unsigned int index);
        
        /**
        * Obtain a reference to the effect channel structure.
        **/
        inline EffectChannel& get_effect_channel(unsigned int index) {
            assert(index < _fx_channels.size());
            return *_fx_channels[index];
        }

        /**
        * Route an instrument channel's output signal to a given effect channel.
        * @param channel_index The index of the channel to route.
        * @param effect_channel_index The index of the effect channel to route to.
        **/
        void route_instrument(unsigned int channel_index, unsigned int effect_channel_index);

        /**
        * Route an effect channel to another.
        * @param effect_channel_src_index The index of the channel to route. Cannot be 0, as that is the master channel.
        * @param effect_channel_dst_index The index of the effect channel to route to.
        **/
        void route_effect(unsigned int effect_channel_src_index, unsigned int effect_channel_dst_index);

        /**
        * Disconnect an effect channel.
        * @param channel_index The index of the effect channel to route. Cannot be 0, as that is the master channel.
        **/
        void disconnect_effect(unsigned int channel_index);

        bool is_note_playable(int key);
    }; // class Song
}