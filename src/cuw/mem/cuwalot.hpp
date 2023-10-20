#pragma once

#include "core.hpp"

#include <cuw/export.hpp>

namespace cuw::mem {
	// standart API
	CUW_EXPORT void* malloc(std::size_t size);
	CUW_EXPORT void* realloc(void* ptr, std::size_t new_size);
	CUW_EXPORT void free(void* ptr);

	// extension API
	CUW_EXPORT void* malloc_ext(std::size_t size, std::size_t alignment = 0, flags_t flags = 0);
	CUW_EXPORT void* realloc_ext(void* ptr, std::size_t old_size, std::size_t new_size, std::size_t alignment = 0, flags_t flags = 0);
	CUW_EXPORT void free_ext(void* ptr, std::size_t size, std::size_t alignment = 0, flags_t flags = 0);
}