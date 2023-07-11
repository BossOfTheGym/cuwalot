#pragma once

#include "core.hpp"

namespace cuw::mem {
	// standart API
	void* malloc(std::size_t size);
	void* realloc(void* ptr, std::size_t new_size);
	void free(void* ptr);

	// extension API
	void* malloc_ext(std::size_t size, std::size_t alignment = 0, flags_t flags = 0);
	void* realloc_ext(void* ptr, std::size_t old_size, std::size_t new_size, std::size_t alignment = 0, flags_t flags = 0);
	void free_ext(void* ptr, std::size_t size, std::size_t alignment = 0, flags_t flags = 0);
}