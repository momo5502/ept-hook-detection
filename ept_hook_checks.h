#pragma once

bool ept_hook_timing_check(void* pointer_in_page);
bool ept_hook_thread_check(void* pointer_in_page);
bool ept_hook_write_check(void* pointer_in_page);