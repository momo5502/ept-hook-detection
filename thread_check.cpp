#include "ept_hook_checks.h"

#include <cstdint>
#include <stdexcept>
#include <thread>
#include <Windows.h>

namespace
{
	// Find a return instruction (0xC3) in the page
	// We can call this instruction as if it was a function
	// That way we can measure execution of a single instruction (+ noise of call instruction)
	void* find_ret_in_page(void* pointer_in_page)
	{
		auto* ptr = reinterpret_cast<uint8_t*>(reinterpret_cast<uint64_t>(pointer_in_page) & ~0xFFF);

		for (size_t i = 0; i < 0x1000; ++i)
		{
			if (ptr[i] == 0xC3)
			{
				return &ptr[i];
			}
		}

		return nullptr;
	}

	// Bind to a specific core to reduce probability of context switches
	void bind_current_thread_to_core(const uint32_t core)
	{
		SetThreadAffinityMask(GetCurrentThread(), 1ull << core);
	}

	// Mark thread as time-critical to reduce probability of context switches
	void set_real_time_prio_for_current_thread()
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	}

	// Measure 'time' of an operation (f) by checking how many times a separate thread can increment a counter
	template <typename F>
	int measure_execution(const F& f)
	{
		volatile bool barrier1 = false;
		volatile bool barrier2 = false;
		volatile bool flag = false;
		volatile int count = 0;

		std::thread t1([&]
		{
			bind_current_thread_to_core(0);
			set_real_time_prio_for_current_thread();

			barrier1 = true;
			while (!barrier2);

			f();

			flag = true;
		});

		std::thread t2([&]
		{
			bind_current_thread_to_core(1);
			set_real_time_prio_for_current_thread();

			int count_local = 0; // Let's hope the compiler allocates this in a register, not on the stack
			barrier2 = true;
			while (!barrier1);

			while (!flag) ++count_local;

			count = count_local;
		});

		t1.join();
		t2.join();

		return count;
	}

	void peform_reads(void* pointer, const uint32_t count)
	{
		for (uint32_t i = 0; i < count; ++i)
		{
			if (*static_cast<volatile uint8_t*>(pointer) != 0xC3)
			{
				throw std::runtime_error("Bad pointer");
			}
		}
	}

	void perform_alternating_read_and_excute(void* pointer, const uint32_t count)
	{
		for (uint32_t i = 0; i < count; ++i)
		{
			if (*static_cast<volatile uint8_t*>(pointer) != 0xC3)
			{
				throw std::runtime_error("Bad pointer");
			}

			static_cast<void(*)()>(pointer)();
		}
	}
#pragma optimize("", on)
}

bool ept_hook_thread_check(void* pointer_in_page)
{
	if (std::thread::hardware_concurrency() == 1)
	{
		throw std::runtime_error("At least 2 cores needed");
	}

	auto* pointer = find_ret_in_page(pointer_in_page);
	if (!pointer)
	{
		throw std::runtime_error("No ret in page");
	}

	// Warmup is not really possible, so just execute often enough
	// to average out the overhead
	constexpr auto execution_count = 1000;

	const auto reads = measure_execution([&]()
	{
		peform_reads(pointer, execution_count);
	});

	auto alts = measure_execution([&]()
	{
		perform_alternating_read_and_excute(pointer, execution_count);
	});

	auto execs = alts - reads;

	// Assuming Time(read) ≈ Time(execution)
	// If executions with preceeding reads take much longer than regular reads
	// it's likely that a hook is present.
	return (reads * 4) < execs;
}
