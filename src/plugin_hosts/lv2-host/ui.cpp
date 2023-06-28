#include "interface.h"
#include "internal.h"
#include "../../util.h"
#include <lv2/ui/ui.h>
#include <suil/suil.h>

using namespace lv2;

static unsigned lilv_ui_supported_func(const char* container_type_uri, const char* ui_type_uri)
{
    dbg("compare ui types %s and %s\n", container_type_uri, ui_type_uri);
    return suil_ui_supported(container_type_uri, ui_type_uri);
}

void UIHost::init(const LilvPlugin* __plugin)
{
    plugin = __plugin;

    LilvUIs* uis = lilv_plugin_get_uis(plugin);
    
    if (uis)
    {
        const LilvUI* target_ui_n = nullptr;
        const char* target_ui_type = nullptr;
        unsigned min_support = 999999; // some arbitrarily high number

        // find the ui widget that is most supported by suil
        LILV_FOREACH (uis, i, uis)
        {
            const LilvUI* ui_n = lilv_uis_get(uis, i);

            const LilvNode* host_type =
    #ifdef UI_WINDOWS
                URI.ui_WindowsUI;
    #else
                URI.ui_X11UI;
    #endif

            const LilvNode* ui_type_n;

            unsigned support = lilv_ui_is_supported(ui_n, suil_ui_supported, host_type, &ui_type_n);

            if (support == 0) continue;
            if (support == 1) {
                target_ui_n = ui_n;
                target_ui_type = lilv_node_as_uri(ui_type_n);
                min_support = 1;
                break;
            } else {
                if (support < min_support) {
                    min_support = support;
                    target_ui_n = ui_n;
                    target_ui_type = lilv_node_as_uri(ui_type_n);
                }
            }
        }

        lilv_uis_free(uis);

        if (target_ui_n)
        {
            dbg("found supported ui %s\n", target_ui_type);
        } else {
            dbg("ui not supported\n");
        }
        
        
    } else {
        dbg("plugin has no uis\n");
    }
}