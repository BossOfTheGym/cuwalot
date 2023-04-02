#include "../../mem_api.hpp"

namespace cuw::mem {
	value_status_t<sysmem_info_t, int> get_sysmem_info() {
		return {};
	}

	value_status_t<void*, int> allocate_sysmem(std::size_t size) {
		return {};
	}

	int deallocate_sysmem(void* ptr, std::size_t size) {
		return {};
	}
}