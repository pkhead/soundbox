#include <iostream>
#include "sys.h"

using namespace sys;

#ifdef _WIN32
#include <Windows.h>

static void lp_time_proc(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	std::function<void()>* callback = static_cast<std::function<void()>*>((void*)dwUser);
	(*callback)();
}

interval_t* sys::set_interval(int ms, std::function<void()> callback_proc)
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
	UINT id = (UINT)interval;
	timeKillEvent(id);
}

#else
#include <thread>
#include <time.h>

struct interval_impl
{
	interval_impl(std::function<void(interval_impl* self)> callback) : thread(callback) { };

	std::thread thread;
	std::atomic<bool> terminate = false;
};

interval_t* sys::set_interval(int ms, std::function<void()> callback_proc)
{
	interval_impl* output = new interval_impl([&](interval_impl* self)
	{
		while (!self->terminate)
		{
			callback_proc();

			timespec time;
			time.tv_sec = 0;
			time.tv_nsec = ms * 1000000;
			nanosleep(&time, nullptr);
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
#endif