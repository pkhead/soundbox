#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <toml11/toml.hpp>
#include <unordered_map>
#include <vector>
#include "imgui.h"
#include "song.h"
#include "toml11/toml/get.hpp"
#include "toml11/toml/value.hpp"
#include "theme.h"

const char* IMGUI_COLOR_NAMES[] = {
    "Text",                 
    "TextDisabled",         
    "WindowBg",             
    "ChildBg",              
    "PopupBg",              
    "Border",               
    "BorderShadow",         
    "FrameBg",              
    "FrameBgHovered",       
    "FrameBgActive",        
    "TitleBg",              
    "TitleBgActive",        
    "TitleBgCollapsed",     
    "MenuBarBg",            
    "ScrollbarBg",          
    "ScrollbarGrab",        
    "ScrollbarGrabHovered", 
    "ScrollbarGrabActive",  
    "CheckMark",            
    "SliderGrab",           
    "SliderGrabActive",     
    "Button",               
    "ButtonHovered",        
    "ButtonActive",         
    "Header",               
    "HeaderHovered",        
    "HeaderActive",         
    "Separator",            
    "SeparatorHovered",     
    "SeparatorActive",      
    "ResizeGrip",           
    "ResizeGripHovered",    
    "ResizeGripActive",     
    "Tab",                  
    "TabHovered",           
    "TabActive",            
    "TabUnfocused",         
    "TabUnfocusedActive",   
    "DockingPreview",       
    "DockingEmptyBg",       
    "PlotLines",            
    "PlotLinesHovered",     
    "PlotHistogram",        
    "PlotHistogramHovered", 
    "TableHeaderBg",        
    "TableBorderStrong",    
    "TableBorderLight",     
    "TableRowBg",           
    "TableRowBgAlt",        
    "TextSelectedBg",       
    "DragDropTarget",       
    "NavHighlight",         
    "NavWindowingHighlight",
    "NavWindowingDimBg",    
    "ModalWindowDimBg",     
};

const char* CUSTOM_COLOR_NAMES[] = {
    "OctaveRow",
    "FifthRow",
    "PianoKey",
    "PianoKeyAccidental",
    "PianoKeyOctave"
};
constexpr size_t NUM_CUSTOM_COLORS = sizeof(CUSTOM_COLOR_NAMES) / sizeof(*CUSTOM_COLOR_NAMES);

/*
const ImGuiCol_ IMGUI_COLOR_ENUMS[] = {
    ImGuiCol_Text,                 
    ImGuiCol_TextDisabled,         
    ImGuiCol_WindowBg,             
    ImGuiCol_ChildBg,              
    ImGuiCol_PopupBg,              
    ImGuiCol_Border,               
    ImGuiCol_BorderShadow,         
    ImGuiCol_FrameBg,              
    ImGuiCol_FrameBgHovered,       
    ImGuiCol_FrameBgActive,        
    ImGuiCol_TitleBg,              
    ImGuiCol_TitleBgActive,        
    ImGuiCol_TitleBgCollapsed,     
    ImGuiCol_MenuBarBg,            
    ImGuiCol_ScrollbarBg,          
    ImGuiCol_ScrollbarGrab,        
    ImGuiCol_ScrollbarGrabHovered, 
    ImGuiCol_ScrollbarGrabActive,  
    ImGuiCol_CheckMark,            
    ImGuiCol_SliderGrab,           
    ImGuiCol_SliderGrabActive,     
    ImGuiCol_Button,               
    ImGuiCol_ButtonHovered,        
    ImGuiCol_ButtonActive,         
    ImGuiCol_Header,               
    ImGuiCol_HeaderHovered,        
    ImGuiCol_HeaderActive,         
    ImGuiCol_Separator,            
    ImGuiCol_SeparatorHovered,     
    ImGuiCol_SeparatorActive,      
    ImGuiCol_ResizeGrip,           
    ImGuiCol_ResizeGripHovered,    
    ImGuiCol_ResizeGripActive,     
    ImGuiCol_Tab,                  
    ImGuiCol_TabHovered,           
    ImGuiCol_TabActive,            
    ImGuiCol_TabUnfocused,         
    ImGuiCol_TabUnfocusedActive,   
    ImGuiCol_DockingPreview,       
    ImGuiCol_DockingEmptyBg,       
    ImGuiCol_PlotLines,            
    ImGuiCol_PlotLinesHovered,     
    ImGuiCol_PlotHistogram,        
    ImGuiCol_PlotHistogramHovered, 
    ImGuiCol_TableHeaderBg,        
    ImGuiCol_TableBorderStrong,    
    ImGuiCol_TableBorderLight,     
    ImGuiCol_TableRowBg,           
    ImGuiCol_TableRowBgAlt,        
    ImGuiCol_TextSelectedBg,       
    ImGuiCol_DragDropTarget,       
    ImGuiCol_NavHighlight,         
    ImGuiCol_NavWindowingHighlight,
    ImGuiCol_NavWindowingDimBg,    
    ImGuiCol_ModalWindowDimBg,     
};
*/

static ImVec4 parse_color(toml::value& value)
{
    if (value.is_array())
    {
        std::vector<toml::value> arr = value.as_array();
        return ImVec4(arr[0].as_floating(), arr[1].as_floating(), arr[2].as_floating(), arr[3].as_floating());
    }
    else
    {
        std::string hex = value.as_string();
        int r, g, b, a;
        sscanf(hex.c_str(), "#%02x%02x%02x%02x", &r, &g, &b, &a);
        return ImVec4((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f);
    }
}

static void parse_toml(Theme* self, toml::value data)
{
    // find channel colors
    auto channel_table = toml::find(data, "channels");

    int i = 1;
    while (true)
    {
        std::string key_name = "Channel_" + std::to_string(i);
        if (!channel_table.contains(key_name)) break;

        std::vector<toml::value>& colors_array =
            toml::find<toml::array>(channel_table, key_name);

        ImVec4 primary_color = parse_color(colors_array[0]);
        ImVec4 secondary_color = parse_color(colors_array[1]);

        self->channel_colors.push_back({
            primary_color,
            secondary_color
        });

        i++;
    }

    // find ui colors
    auto ui_table = toml::find(data, "ui");

    for (size_t i = 0; i < sizeof(IMGUI_COLOR_NAMES) / sizeof(*IMGUI_COLOR_NAMES); i++)
    {
        const char* name = IMGUI_COLOR_NAMES[i];

        if (!ui_table.contains(name))
        {
            std::cout << "did not find " << name << "\n";
            continue;
        }

        ImVec4 color = parse_color(toml::find(ui_table, name));
        self->ui_colors[name] = color;
    }

    // find custom colors
    for (size_t i = 0; i < NUM_CUSTOM_COLORS; i++)
    {
        const char* name = CUSTOM_COLOR_NAMES[i];

        if (!ui_table.contains(name))
        {
            std::cout << "did not find " << name << "\n";
            continue;
        }

        ImVec4 color = parse_color(toml::find(ui_table, name));
        self->custom_colors[i] = color;
    }
}

Theme::Theme()
{
    // use default colors
    ImGuiStyle style;
    ImGui::StyleColorsClassic(&style);
    ImVec4* colors = style.Colors;

    for (int i = 0; i < 55; i++)
    {
        const std::string str = IMGUI_COLOR_NAMES[i];
        ui_colors[str] = colors[i];
    }

    channel_colors.push_back({
        ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
        ImVec4(0.5f, 0.5f, 0.5f, 1.0f)
    });
}

Theme::Theme(std::istream& stream)
{
    auto data = toml::parse(stream, "unknown file");
    parse_toml(this, data);
}

Theme::Theme(const std::string file_name)
{
    auto data = toml::parse(file_name);
    parse_toml(this, data);
}

ImU32 Theme::get_channel_color(int ch, bool is_primary) const
{
    const ChannelColor& color_struct = channel_colors[ch % channel_colors.size()]; 
    return ImGui::ColorConvertFloat4ToU32(is_primary ? color_struct.primary : color_struct.secondary);
}

ImVec4 Theme::get_custom_color(CustomColor color) const
{
    return custom_colors[static_cast<size_t>(color)];
}

void Theme::set_imgui_colors() const
{
    ImVec4* style = ImGui::GetStyle().Colors;
    for (int i = 0; i < 55; i++)
    {
        const std::string str = IMGUI_COLOR_NAMES[i];

        // if ui_colors contains key
        if (ui_colors.find(str) != ui_colors.end())
        {
            style[i] = ui_colors.at(str);
        }
    }
}