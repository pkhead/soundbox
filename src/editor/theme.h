#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <imgui.h>
#include <tomlcpp/tomlcpp.hpp>

enum class CustomColor : uint8_t
{
    OctaveRow = 0,
    FifthRow = 1,
    PianoKey = 2,
    PianoKeyAccidental = 3,
    PianoKeyOctave = 4,
};

class Theme
{
private:
    std::string _name;
    void _parse_toml(toml::Table* table);
public:
    struct ChannelColor
    {
        ImVec4 primary;
        ImVec4 secondary;
    };

    std::vector<ChannelColor> channel_colors;
    std::unordered_map<std::string, ImVec4> ui_colors;
    ImVec4 custom_colors[5];

    Theme();
    Theme(const std::string theme_name);
    Theme(std::istream& stream);

    void load(const std::string theme_name);
    void set_imgui_colors() const;
    ImU32 get_channel_color(int ch, bool is_primary) const;
    ImVec4 get_custom_color(CustomColor color) const;

    void scan_themes();
    inline const std::string& name() const { return _name; }; 
    const std::vector<std::string>& get_themes_list() const;
};