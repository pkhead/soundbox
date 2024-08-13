#pragma once
#include "audio_engine/audio_engine.hpp"
#include "shortcuts.hpp"
#include "song.hpp"
#include "song_editor.hpp"

namespace sbox
{
    struct ModuleList
    {

    public:
        struct ModuleCategory
        {
            std::string name;
            std::vector<modules::ModuleInfo> modules;

            inline ModuleCategory(const std::string &name) :
                name(name)
            {}
        };

        void module_list_by_host(const std::vector<modules::ModuleInfo> &info_list);
        void module_list_by_author(const std::vector<modules::ModuleInfo> &info_list);

        typedef std::vector<ModuleCategory>::iterator iterator;
        typedef std::vector<ModuleCategory>::const_iterator const_iterator;
        typedef std::vector<ModuleCategory>::value_type value_type;

        inline iterator begin() { return module_list.begin(); }
        inline iterator end() { return module_list.end(); }
        inline const_iterator cbegin() { return module_list.cbegin(); }
        inline const_iterator cend() { return module_list.cend(); }

    private:
        std::vector<ModuleCategory> module_list;
    };

    class ModuleEditor
    {
    private:
        void render_channel_settings(Channel &channel);

    public:
        enum ChannelType
        {
            CHANNEL_TYPE_INSTRUMENT,
            CHANNEL_TYPE_EFFECT
        } channel_type;
        int selected_channel;
        
        SongEditor &editor;
        ModuleList &module_list;

        ModuleEditor(SongEditor &editor, ModuleList &mod_list);

        void draw(const char *window_title);
    }; // class ModuleEditor
} // namespace sbox