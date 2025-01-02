#include <iostream>
#include <cassert>
#include <atomic>
#include "sys.hpp"

using namespace sys;

bool sys::IS_BIG_ENDIAN;

void sys::query_endianness() {
    union {
        uint32_t i;
        char c[4];
    } static bint = {0x01020304};

    sys::IS_BIG_ENDIAN = bint.c[0] == 1;
}

#ifdef _WIN32
#include <windows.h>
#include <avrt.h>

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

struct SleepHandleInternal
{
    HANDLE avrt_api_handle;
    HANDLE timer;
};

SleepHandle::SleepHandle()
{
    SleepHandleInternal *internal = new SleepHandleInternal;

    DWORD task_index = 0;
    internal->avrt_api_handle = AvSetMmThreadCharacteristics("Pro Audio", &task_index);

    if (_handle == nullptr)
    {
        const char* errmsg = "(unknown error)";

        switch (GetLastError()) {
            case ERROR_INVALID_TASK_INDEX:
                errmsg = "ERROR_INVALID_TASK_INDEX";
                break;

            case ERROR_INVALID_TASK_NAME:
                errmsg = "ERROR_INVALID_TASK_NAME";
                break;

            case ERROR_PRIVILEGE_NOT_HELD:
                errmsg = "ERROR_PRIVILEGE_NOT_HELD";
                break;
        }

        throw std::runtime_error(std::string("could not create SleepHandle: ") + errmsg);
    }

    if (!(internal->timer = CreateWaitableTimer(NULL, true, NULL)))
    {
        throw std::runtime_error("could not create SleepHandle");
    }

    _handle = internal;
}

SleepHandle::~SleepHandle()
{
    assert(_handle != nullptr);
    SleepHandleInternal *internal = (SleepHandleInternal*) _handle;

    CloseHandle(internal->timer);
    AvRevertMmThreadCharacteristics(internal->avrt_api_handle);
    delete internal;
}

void SleepHandle::sleep(unsigned long ms)
{
    assert(_handle != nullptr);
    SleepHandleInternal *internal = (SleepHandleInternal*) _handle;

    TIMECAPS tc;
    UINT     wTimerRes;

    if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR) 
    {
        throw std::runtime_error("error while sleeping");
    }

    wTimerRes = min(max(tc.wPeriodMin, 1), tc.wPeriodMax);
    timeBeginPeriod(wTimerRes); 

    LARGE_INTEGER li;
    li.QuadPart = -(LONGLONG)(ms * 1e4);
    if (!SetWaitableTimer(internal->timer, &li, 0, NULL, NULL, FALSE))
        throw std::runtime_error("error while sleeping");

    WaitForSingleObject(internal->timer, INFINITE);

    timeEndPeriod(wTimerRes);
}

#else
#include <dlfcn.h>
#include <chrono>
#include <unistd.h>

SleepHandle::SleepHandle()
{
    _handle = nullptr;
}

SleepHandle::~SleepHandle()
{

}

void SleepHandle::sleep(unsigned long ms)
{
    usleep(ms * 1000);
}

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
