#include <algorithm>
#include <chrono>
#include <dlfcn.h>
#include <iostream>
#include <atomic>
#include "sys.h"

using namespace sys;

#ifdef _WIN32
#include <windows.h>

static void lp_time_proc(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	std::function<void()>* callback = static_cast<std::function<void()>*>((void*)dwUser);
	(*callback)();
}

interval_t* sys::set_interval(int ms, const std::function<void()>&& callback_proc)
{
	return (interval_t*)timeSetEvent(
		ms,
		ms,
		lp_time_proc,
		(DWORD_PTR)(new std::function(callback_proc)),
		TIME_PERIODIC
	);
}

void sys::clear_interval(interval_t* interval)
{
	// TODO: memory leak, did not delete std::function userdata
	// not worth fixing for now because only one interval is ever created
	UINT id = (UINT)(size_t)interval;
	timeKillEvent(id);
}

#else
#include <thread>
#include <time.h>

struct interval_impl
{
	interval_impl(std::function<void(interval_impl* self)>&& callback) : terminate(false), thread(callback, this) { };

	std::thread thread;
	std::atomic<bool> terminate;
};

interval_t* sys::set_interval(int ms, const std::function<void()>&& callback_proc)
{
	interval_impl* output = new interval_impl([ms, callback_proc](interval_impl* self)
	{
		while (!self->terminate)
		{
			callback_proc();
			usleep(ms * 1000);
		}
	});

	return (interval_t*) output;
}

void sys::clear_interval(interval_t* interval)
{
	interval_impl* impl = (interval_impl*)interval;
	impl->terminate = true;
	impl->thread.join();
	delete impl;
}

dl_handle sys::dl_open(const char *file_path)
{
	return dlopen(file_path, RTLD_NOW);
}

int sys::dl_close(dl_handle handle)
{
	return dlclose(handle);
}

void* sys::dl_sym(dl_handle handle, const char *symbol_name)
{
	return dlsym(handle, symbol_name);
}

char* sys::dl_error()
{
	return dlerror();
}

#endif
