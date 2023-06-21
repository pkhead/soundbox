#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <imgui.h>

class Theme
{
public:
    struct ChannelColor
    {
        ImVec4 primary;
        ImVec4 secondary;
    };

    std::vector<ChannelColor> channel_colors;
    std::unordered_map<std::string, ImVec4> ui_colors;

    Theme();
    Theme(const std::string file_name);
    Theme(std::istream& stream);

    void set_imgui_colors() const;
    ImU32 get_channel_color(int ch, bool is_primary) const;
};