#pragma once
#include "../modules.hpp"

namespace hosts::internal
{
    /**
    * Module host for stock plugins.
    **/
    class InternalModuleHost : public modx::ModuleHost
    {
    public:
        const char* host_id() const override;
        bool initialize() override;
        const std::vector<std::string> scan_modules() override;
        bool create_module(modules::ModuleCreator &creator) override;
    };
}