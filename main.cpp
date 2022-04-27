#include <Windows.h>

#include <thread>
#include "ept_hook_checks.h"

using namespace std::literals;

void hook_me();

int main()
{
	printf("PID: %ld\n\n", GetCurrentProcessId());

	while (true)
	{
		hook_me();

		const bool timing = ept_hook_timing_check(&hook_me);
		printf("Timing:\t\t%s\n", timing ? "Yes" : "No");

		const bool thread = ept_hook_thread_check(&hook_me);
		printf("Thread:\t\t%s\n", thread ? "Yes" : "No");

		const bool writing = ept_hook_write_check(&hook_me);
		printf("Writing:\t%s\n", writing ? "Yes" : "No");

		printf("\n");

		std::this_thread::sleep_for(1s);
	}

	return 0;
}

// Force the code below to be emitted in a new page
#pragma code_seg(".hookme")

__declspec(dllexport) volatile bool _do = true;

#pragma optimize("", off)
void hook_me()
{
	if (_do)
	{
		printf("Hooked:\t\tNo\n");
	}
	else
	{
		printf("Hooked:\t\tYes\n");
	}
}

// With a bit of luck the alignment of this function causes
// a few 0xCC instructions to be emitted.
__declspec(dllexport) void some_other_function_in_the_same_page()
{
	
}
#pragma optimize("", on)