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

high_res_timer* sys::timer_create(int ms)
{
	timer_impl* impl = new timer_impl;
	impl->resolution = ms;

	if (timeBeginPeriod(ms) != TIMERR_NOERROR)
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

	CloseHandle(impl->handle);
	delete impl;
}

void sys::timer_sleep(high_res_timer* timer, int ms)
{
	timer_impl* impl = (timer_impl*) timer;

	LARGE_INTEGER li;
	li.QuadPart = -10000 * (LONGLONG)ms;
	if (!SetWaitableTimer(impl->handle, &li, 0, NULL, NULL, FALSE)) {
		std::cerr << "error: could not set timer\n";
		return;
	}

	WaitForSingleObject(impl->handle, INFINITE);
}
#endif