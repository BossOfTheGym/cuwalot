#include "../../mem_api.hpp"

#include <windows.h>
#include <memoryapi.h>

namespace cuw::mem {
	value_status_t<sysmem_info_t, int> get_sysmem_info() {
		SYSTEM_INFO info{};
		GetNativeSystemInfo(&info);
		return {{info.dwPageSize}, 0};
	}

	value_status_t<void*, int> allocate_sysmem(std::size_t size) {
		if (void* ptr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)) {
			return {ptr, 0};
		} return {nullptr, -1};
	}

	int deallocate_sysmem(void* ptr, std::size_t size) {
		VirtualFree(ptr, 0, MEM_RELEASE);
		return 0;
	}
}