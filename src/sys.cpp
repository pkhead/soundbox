#include <iostream>
#include "sys.h"

using namespace sys;

#ifdef _WIN32
#include <Windows.h>

struct timer_impl
{
	int resolution;
	HANDLE handle;
};

high_res_timer* sys::timer_create(long us)
{
	timer_impl* impl = new timer_impl;
	impl->resolution = us * 1000;

	if (timeBeginPeriod(impl->resolution) != TIMERR_NOERROR)
		goto error;

	if (!(impl->handle = CreateWaitableTimer(NULL, true, NULL)))
		goto error;

	return (high_res_timer*) impl;

error:
	std::cerr << "error: could not create timer\n";
	delete impl;
	return nullptr;
}

void sys::timer_free(high_res_timer* timer)
{
	timer_impl* impl = (timer_impl*) timer;

	timeEndPeriod(impl->resolution);

	CloseHandle(impl->handle);
	delete impl;
}

void sys::timer_sleep(high_res_timer* timer, long us)
{
	timer_impl* impl = (timer_impl*) timer;

	LARGE_INTEGER li;
	li.QuadPart = -10 * (LONGLONG)us;
	if (!SetWaitableTimer(impl->handle, &li, 0, NULL, NULL, FALSE)) {
		std::cerr << "error: could not set timer\n";
		return;
	}

	WaitForSingleObject(impl->handle, INFINITE);
}

static void lp_time_proc(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	std::function<void()>* callback = static_cast<std::function<void()>*>((void*)dwUser);
	(*callback)();
}

interval_t* sys::set_interval(int resolution_ms, int ms, std::function<void()> callback_proc)
{
	return (interval_t*)timeSetEvent(
		ms,
		resolution_ms,
		lp_time_proc,
		(DWORD_PTR)(new std::function(callback_proc)),
		TIME_PERIODIC
	);
}

void sys::clear_interval(interval_t* interval)
{
	UINT id = (UINT)interval;
	timeKillEvent(id);
}

#else
#include <time.h>

high_res_timer* sys::timer_create(long us)
{
	return nullptr;
}

void sys::timer_free(high_res_timer* timer)
{
	return;
}

void sys::timer_sleep(high_res_timer* timer, long us)
{
	timespec time;
	time.tv_sec = 0;
	time.tv_nsec = us * 1000;
	nanosleep(&time, nullptr);
}
#endif