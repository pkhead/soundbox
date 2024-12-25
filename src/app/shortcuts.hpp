#include <string>
#include <unordered_map>
#include <imgui.h>
#pragma once

namespace sbox
{
    enum class ShortcutID : unsigned int
    {
        // application control
        QUIT,

        // playhead controls
        PLAY_PAUSE, PLAYHEAD_NEXT, PLAYHEAD_PREV, PLAYHEAD_TO_FIRST, PLAYHEAD_TO_CURSOR,

        CURSOR_LEFT, CURSOR_UP, CURSOR_RIGHT, CURSOR_DOWN,

        // not an actual enum, just the number of values.
        COUNT,
        NONE = (unsigned int)-1,
    };

    enum class ModKeys : int
    {
        NONE = 0,
        CTRL = 1,
        SHIFT = 2,
        ALT = 4,
    };

    class ShortcutContext
    {
    private:
        struct Binding
        {
            ShortcutID id;
            std::string name;
            std::string shortcut_string;
            ImGuiKey key;
            ModKeys mods;

            bool is_activated;
            bool is_deactivated;
            bool is_active;
            bool allow_repeat;
        };

        void bind(const std::string &name, ShortcutID id, ModKeys mods, ImGuiKey key, bool allow_repeat = false);
        static std::string generate_shortcut_string(ModKeys mods, ImGuiKey key);

        bool is_key_pressed(const Binding &binding);
        bool is_key_down(const Binding &binding);

        std::unordered_map<ShortcutID, Binding> _key_shortcuts;
    public:
        ShortcutContext();

        inline bool is_activated(ShortcutID id) const {
            return _key_shortcuts.at(id).is_activated;
        }

        inline bool is_active(ShortcutID id) const {
            return _key_shortcuts.at(id).is_active;
        }

        inline void activate(ShortcutID id) {
            _key_shortcuts[id].is_activated = true;
        }

        void imgui_menu_item(ShortcutID id, const std::string &name, bool selected = false);

        void update();
    };
}