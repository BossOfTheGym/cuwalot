#include "../../mem_api.hpp"

#include <unistd.h>
#include <sys/mman.h>

namespace cuw::mem {
	value_status_t<sysmem_info_t, int> get_sysmem_info() {
		int page_size = sysconf(_SC_PAGE_SIZE);
		if (page_size == -1) {
			return {{}, -1};
		}
		return {{ .page_size = page_size }, 0};
	}

	value_status_t<void*, int> allocate_sysmem(std::size_t size) {
		void* memory = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (memory != MAP_FAILED) {
			return {memory, 0};
		}
		return {nullptr, -1};
	}

	int deallocate_sysmem(void* ptr, std::size_t size) {
		return munmap(ptr, size);
	}
}