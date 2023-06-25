#include "lv2.h"
using namespace plugins;

static LilvWorld* LILV_WORLD;

void Lv2Plugin::lilv_init() {
    LILV_WORLD = lilv_world_new();
}

void Lv2Plugin::lilv_fini() {
    lilv_world_free(LILV_WORLD);
}

const char* Lv2Plugin::get_standard_paths()
{
    const char* list_str = std::getenv("LV2_PATH");
    if (list_str == nullptr)
        list_str =
#ifdef __APPLE__
            "~/.lv2:~/Library/Audio/Plug-Ins/LV2:/usr/local/lib/lv2:/usr/lib/lv2:/Library/Audio/Plug-Ins/LV2";
#elif defined(_WIN32)
            "%APPDATA%\\LV2;%COMMONPROGRAMFILES%\\LV2"
#else
            "~/.lv2:/usr/local/lib/lv2:/usr/lib/lv2";
#endif
    // haiku paths are defined in lilv source files as "~/.lv2:/boot/common/add-ons/lv2"

    return list_str;
}

void Lv2Plugin::scan_plugins(const std::vector<std::string> &paths, std::vector<PluginData> &data_out)
{
    // create path string to be parsed by lilv
    const char delim =
#ifdef _WIN32
        ';';
#else
        ':';
#endif
    std::string path_str;

    for (int i = 0; i < paths.size(); i++) {
        if (i != 0) path_str += delim;
        path_str += paths[i];
    }

    LilvNode* lv2_path = lilv_new_file_uri(LILV_WORLD, NULL, path_str.c_str());
    lilv_world_set_option(LILV_WORLD, LILV_OPTION_LV2_PATH, lv2_path);
    lilv_node_free(lv2_path);
}