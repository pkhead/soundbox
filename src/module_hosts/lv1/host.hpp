#pragma once
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <sys.hpp>
#include "../modules.hpp"

namespace hosts::lv1
{
    /**
    * Module host for LADPSA/LV1 plugins
    **/
    class Lv1ModuleHost : public modx::ModuleHost
    {
    private:
        struct PluginInfo
        {
            std::filesystem::path dl_path;
            sys::dl_handle dl;
            unsigned long index;

            PluginInfo(std::filesystem::path dl_path, sys::dl_handle &&dl, unsigned long index) :
                dl_path(dl_path),
                dl(std::move(dl)),
                index(index)
            {}
        };

        std::unordered_map<std::string, std::unique_ptr<PluginInfo>> _plugin_info;
        void get_plugin_info(std::filesystem::path dlpath, std::vector<modules::ModuleInfo> &out_mod_list);
    public:
        std::vector<std::filesystem::path> search_paths;

        const char* host_id() const override;
        bool initialize() override;
        const std::vector<modules::ModuleInfo> scan_modules() override;
        bool create_module(modules::ModuleCreator &create) override;
    }; // namespace Lv1ModuleHost
} // namespace hosts::lv1