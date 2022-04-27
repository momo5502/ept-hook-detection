#include "ept_hook_checks.h"

#include <cstdint>
#include <intrin.h>
#include <stdexcept>

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

#pragma optimize("", off)
	uint64_t measure_execution(void* pointer)
	{
		const auto start = __rdtsc();
		static_cast<void(*)()>(pointer)();
		return __rdtsc() - start;
	}

	uint64_t measure_read(void* pointer)
	{
		auto start = __rdtsc();
		if (*static_cast<uint8_t*>(pointer) != 0xC3)
		{
			throw std::runtime_error("Bad pointer");
		}
		return __rdtsc() - start;
	}
#pragma optimize("", on)

	uint64_t measure_reads(void* pointer, const uint32_t count)
	{
		uint64_t avg = 0;
		measure_read(pointer); // Warmup

		for (uint32_t i = 0; i < count; ++i)
		{
			avg += measure_read(pointer);
		}

		return avg / count;
	}

	uint64_t measure_executions_with_alternating_reads(void* pointer, const uint32_t count)
	{
		uint64_t exec_avg = 0, read_avg = 0;
		measure_read(pointer); // Warmup
		measure_execution(pointer); // Warmup

		for (uint32_t i = 0; i < count; ++i)
		{
			read_avg += measure_read(pointer);
			exec_avg += measure_execution(pointer);
		}

		(void)read_avg;
		return exec_avg / count;
	}
}

bool ept_hook_timing_check(void* pointer_in_page)
{
	auto* pointer = find_ret_in_page(pointer_in_page);
	if (!pointer)
	{
		throw std::runtime_error("No ret in page");
	}
	
	const auto reads = measure_reads(pointer, 20);
	const auto execs = measure_executions_with_alternating_reads(pointer, 20);

	// Assuming Time(read) ≈ Time(execution)
	// If executions with preceeding reads take much longer than regular reads
	// it's likely that a hook is present.
	return (reads * 4) < execs;
}
