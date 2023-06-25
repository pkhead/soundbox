#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <tomlcpp/tomlcpp.hpp>
#include <unordered_map>
#include <vector>
#include <imgui.h>
#include <filesystem>
#include "../song.h"
#include "portaudio.h"
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

static ImVec4 parse_color(const std::string& hex)
{
    int r, g, b, a;
    sscanf(hex.c_str(), "#%02x%02x%02x%02x", &r, &g, &b, &a);
    return ImVec4((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f);
}

void Theme::_parse_toml(toml::Table* data)
{
    // find tables
    auto channel_table = data->getTable("channels");
    auto ui_table = data->getTable("ui");

    if (!channel_table)
        throw std::runtime_error("could not find [channel]");

    if (!ui_table)
        throw std::runtime_error("could not find [ui]");
    
    int i = 1;
    while (true)
    {
        std::string key_name = "Channel_" + std::to_string(i);
        if (!channel_table->getArray(key_name)) break;

        auto colors_array =
            channel_table->getArray(key_name);
        
        auto [ ok0, color0 ]  = colors_array->getString(0);
        auto [ ok1, color1 ] = colors_array->getString(1);

        if (!ok0 || !ok1)
            throw std::runtime_error("invalid channel color array");
        
        ImVec4 primary_color = parse_color(color0);
        ImVec4 secondary_color = parse_color(color1);

        channel_colors.push_back({
            primary_color,
            secondary_color
        });

        i++;
    }

    // read ui colors
    for (size_t i = 0; i < sizeof(IMGUI_COLOR_NAMES) / sizeof(*IMGUI_COLOR_NAMES); i++)
    {
        const char* name = IMGUI_COLOR_NAMES[i];
        auto [ok, color_value] = ui_table->getString(name);

        if (!ok)
        {
            std::cout << "did not find " << name << "\n";
            continue;
        }

        ImVec4 color = parse_color(color_value);
        ui_colors[name] = color;
    }

    // find custom colors
    for (size_t i = 0; i < NUM_CUSTOM_COLORS; i++)
    {
        const char* name = CUSTOM_COLOR_NAMES[i];
        auto [ok, color_value] = ui_table->getString(name);

        if (!ok)
        {
            std::cout << "did not find " << name << "\n";
            continue;
        }

        ImVec4 color = parse_color(color_value);
        custom_colors[i] = color;
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
    std::string conf(std::istreambuf_iterator<char>{stream}, {});
    auto res = toml::parse(conf);
    if (!res.table)
        throw std::runtime_error("cannot parse stream: " + res.errmsg);

    _parse_toml(res.table.get());
}

const char PATH_SEP =
#ifdef _WIN32
    '\\';
#else
    '/';
#endif

Theme::Theme(const std::string theme_name)
{
    load(theme_name);
}

void Theme::load(const std::string theme_name)
{
    channel_colors.clear();
    ui_colors.clear();

    _name = theme_name;
    std::string file_name = std::string("styles") + PATH_SEP + theme_name + ".toml";

    auto res = toml::parseFile(file_name);
    if (!res.table)
        throw std::runtime_error("cannot parse " + file_name + ": " + res.errmsg);
    
    _parse_toml(res.table.get());
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

// Theme discovery //
static std::vector<std::string> theme_names;

void Theme::scan_themes()
{
    theme_names.clear();

    for (const auto& entry : std::filesystem::directory_iterator("styles"))
    {
        auto& path = entry.path();

        if (path.extension() == ".toml")
        {
            std::cout << path.stem() << "\n";
            theme_names.push_back(path.stem().string());
        }
    }
}

const std::vector<std::string>& Theme::get_themes_list() const
{
    return theme_names;
}