#include <tuple>
#include <cstring>
#include <iostream>

#include <cuw/mem/mem_api.hpp>

using namespace cuw;

namespace {
	int test_sysmem_info() {
		auto [mem_info, status] = mem::get_sysmem_info();
		if (status) {
			std::cerr << "failed to obtain sysmem info" << std::endl;
			return 1;
		}
		std::cout << "page size: " << mem_info.page_size << std::endl;
		std::cout << "test passed" << std::endl;
		std::cout << std::endl;
		return 0;
	}

	int test_alloc_free() {
		auto mem_info = mem::get_sysmem_info();
		if (mem_info.status) {
			std::cerr << "failed to obtain sysmem info" << std::endl;
			return 1;
		}

		std::cout << "page_size: " << mem_info.value.page_size << std::endl;

		std::size_t alloc_size = 1234567;
		std::cout << "alloc_size: " << alloc_size << std::endl;
		alloc_size = (alloc_size + mem_info.value.page_size - 1) & ~(mem_info.value.page_size - 1);
		std::cout << "alloc_size aligned: " << alloc_size << std::endl;

		auto ptr = mem::allocate_sysmem(alloc_size);
		if (ptr.status) {
			std::cerr << "failed to allocate system memory" << std::endl;
			return 1;
		} std::cout << "ptr: " << ptr.value << std::endl;

		std::cout << "writing memory..." << std::endl;
		std::memset(ptr.value, 0xFF, alloc_size);

		std::cout << "reading memory..." << std::endl;
		for (std::uint8_t* start = (std::uint8_t*)ptr.value, *end = start + alloc_size; start != end; ++start) {
			if (*start != 0xFF) {
				std::cerr << "invalid read" << std::endl;
				return 1;
			}
		} std::cout << "read successful" << std::endl;

		std::cout << "deallocating..." << std::endl;
		if (mem::deallocate_sysmem(ptr.value, alloc_size)) {
			std::cerr << "failed to deallocate system memory" << std::endl;
			return 1;
		}

		std::cout << "test passed" << std::endl;
		std::cout << std::endl;
		return 0;
	}
}

int main(int argc, char* argv[]) {
	if (int status = test_sysmem_info(); status) {
		return status;
	} if (int status = test_alloc_free(); status) {
		return status;
	}return 0;
}