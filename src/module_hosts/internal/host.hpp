#pragma once
#include <array>
#include "../modules.hpp"

namespace hosts::internal
{
    /**
    * Module host for stock plugins.
    **/
    class InternalModuleHost : public modx::ModuleHost
    {
    public:
        static const std::array<std::string, 2> hidden_mod_classes;

        const char* host_id() const override;
        bool initialize() override;
        const std::vector<modules::ModuleInfo> scan_modules() override;
        bool create_module(modules::ModuleCreator &creator) override;
    };
}