#pragma once

#include <cstddef>

#include <cuw/utils/utils.hpp>

namespace cuw::mem {
	struct sysmem_info_t {
		int page_size{-1};
	};

	// 0 - success, -1 - failure
	value_status_t<sysmem_info_t, int> get_sysmem_info();

	// 0 - success, -1 - failure
	value_status_t<void*, int> allocate_sysmem(std::size_t size);

	// 0 - success, -1 - failure
	int deallocate_sysmem(void* ptr, std::size_t size);
}