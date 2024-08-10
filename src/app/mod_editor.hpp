#pragma once
#include "audio_engine/audio_engine.hpp"
#include "shortcuts.hpp"
#include "song.hpp"

namespace sbox
{
    class ModuleEditor
    {
    private:
        struct ModuleCategory
        {
            std::string name;
            std::vector<modules::ModuleInfo> modules;
        };

        std::vector<ModuleCategory> _module_list;

        void module_list_by_host();
        void module_list_by_author();
    public:
        enum ChannelType
        {
            CHANNEL_TYPE_INSTRUMENT,
            CHANNEL_TYPE_EFFECT
        } selected_channel_type;
        int selected_channel;

        Song &song;
        ShortcutContext &shortcuts;

        ModuleEditor(Song& song, ShortcutContext &shortcuts);

        void draw();
    }; // class ModuleEditor
} // namespace sbox