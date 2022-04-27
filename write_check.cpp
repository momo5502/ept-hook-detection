#include "ept_hook_checks.h"

#include <cstdint>
#include <stdexcept>

#include <Windows.h>

namespace
{
	// Find a CC instruction in the page we can overwrite.
	// A CC following another CC should never be executed,
	// as the first CC causes a crash in case of execution
	void* find_double_cc_in_page(void* pointer_in_page)
	{
		auto* ptr = reinterpret_cast<uint8_t*>(reinterpret_cast<uint64_t>(pointer_in_page) & ~0xFFF);

		for (size_t i = 0; i < (0x1000 - 1); ++i)
		{
			if (ptr[i] == 0xCC && ptr[i + 1] == 0xCC)
			{
				return &ptr[i + 1];
			}
		}

		return nullptr;
	}

	bool execute_throws(void* pointer)
	{
		__try
		{
			static_cast<void(*)()>(pointer)();
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return true;
		}

		return false;
	}

	void write_byte(void* pointer, const uint8_t byte)
	{
		DWORD old{};
		VirtualProtect(pointer, 1, PAGE_EXECUTE_READWRITE, &old);

		*static_cast<uint8_t*>(pointer) = byte;

		VirtualProtect(pointer, 1, old, &old);
	}
}

bool ept_hook_write_check(void* pointer_in_page)
{
	auto* pointer = find_double_cc_in_page(pointer_in_page);
	if (!pointer)
	{
		throw std::runtime_error("No double cc in page");
	}

	if(!execute_throws(pointer))
	{
		// We read CC but did not throw? Probably already hooked?
		// Or exception handling is broken.
		throw std::runtime_error("CC did not throw");
	}

	write_byte(pointer, 0xC3);
	const bool did_throw =  execute_throws(pointer);
	write_byte(pointer, 0xCC);

	// If we still did throw even though we wrote C3,
	// it's certain that a hook is installed.
	return did_throw;
}
