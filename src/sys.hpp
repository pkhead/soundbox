#pragma once
#include <filesystem>

namespace sys
{
    class dl_handle
    {
    private:
        void* _handle;
        dl_handle(const std::filesystem::path &file_path);

    public:
        dl_handle(const dl_handle&) = delete;
        dl_handle& operator=(const dl_handle&) = delete;
        dl_handle& operator=(dl_handle&&) = default;
        dl_handle(dl_handle&&);
        ~dl_handle();

        inline static dl_handle open(const std::filesystem::path &file_path) { return dl_handle(file_path); }
        inline bool is_open() const { return _handle != nullptr; }
        static const char* error();

        void* sym(const char *symbol_name) const;
    }; // class dl_handle
}