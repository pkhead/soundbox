#pragma once
#include <filesystem>

namespace sys
{
    extern bool IS_BIG_ENDIAN;

    /**
     * Provides functionality for high-resolution sleeping.
     * Only one may exist per thread.
     */
    class SleepHandle
    {
    private:
        void* _handle;
    
    public:
        SleepHandle();
        ~SleepHandle();

        /**
         * Sleep for the provided number of milliseconds.
         * @param ms The number of milliseconds to sleep.
         */
        void sleep(unsigned long ms);
    }; // class SleepHandle

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
