#include <iostream>
#include <cassert>
#include <atomic>
#include "sys.hpp"

using namespace sys;

#ifdef _WIN32
#include <windows.h>

dl_handle::dl_handle(const std::filesystem::path &file_path)
{
    _handle = (void*) LoadLibraryW(file_path.c_str());
}

dl_handle::dl_handle(dl_handle &&src)
{
    _handle = src._handle;
    src._handle = nullptr;
}

dl_handle::~dl_handle()
{
    if (_handle)
        FreeLibrary((HMODULE) _handle);
}

void* dl_handle::sym(const char *symbol_name) const
{
    assert(_handle);
    return (void*) GetProcAddress((HMODULE) _handle, symbol_name);
}

const char* dl_handle::error()
{
	return "Win32 error messages unimplemented";
}

#else
#include <dlfcn.h>

dl_handle::dl_handle(const std::filesystem::path &file_path)
{
    static_assert(std::is_same<std::filesystem::path::value_type, char>(), "on non-windows platform, but std::filesystem::path::value_type is not char");
    _handle = dlopen(file_path.c_str(), RTLD_NOW);
}

dl_handle::dl_handle(dl_handle &&src)
{
    _handle = src._handle;
    src._handle = nullptr;
}

dl_handle::~dl_handle()
{
    if (_handle)
        dlclose(_handle);
}

void* dl_handle::sym(const char *symbol_name) const
{
    assert(_handle);
	return dlsym(_handle, symbol_name);
}

const char* dl_handle::error()
{
	return dlerror();
}

#endif
