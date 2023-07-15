#include <tuple>
#include <cstring>
#include <iostream>

#include <cuw/utils/ptr.hpp>
#include <cuw/mem/mem_api.hpp>

#include "utils.hpp"

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
		}
		std::cout << "ptr: " << ptr.value << std::endl;

		std::cout << "writing memory..." << std::endl;
		std::memset(ptr.value, 0xFF, alloc_size);

		std::cout << "reading memory..." << std::endl;
		for (std::uint8_t* start = (std::uint8_t*)ptr.value, *end = start + alloc_size; start != end; ++start) {
			if (*start != 0xFF) {
				std::cerr << "invalid read" << std::endl;
				return 1;
			}
		}
		std::cout << "read successful" << std::endl;

		std::cout << "deallocating..." << std::endl;
		if (mem::deallocate_sysmem(ptr.value, alloc_size)) {
			std::cerr << "failed to deallocate system memory" << std::endl;
			return 1;
		}

		std::cout << "test passed" << std::endl;
		std::cout << std::endl;
		return 0;
	}

	int test_memory_allocations() {
		std::cout << "testing memory allocations..." << std::endl;

		auto mem_info = mem::get_sysmem_info();
		if (mem_info.status) {
			std::cerr << "failed to obtain sysmem info" << std::endl;
			return 1;
		}

		std::cout << "page_size: " << mem_info.value.page_size << std::endl;

		std::size_t alloc_size = mem_info.value.page_size;

		std::cout << "consequitive allocations" << std::endl;
		void* allocations[16] = {};
		for (auto& allocation : allocations) {
			if (auto [ptr, status] = mem::allocate_sysmem(alloc_size); !status) {
				allocation = ptr;
			} else {
				std::cerr << "failed to allocate system memory" << std::endl;
				return 1;
			}
		}

		for (auto& allocation : allocations) {
			std::cout << print_range_t{allocation, alloc_size} << std::endl;
		}

		for (auto& allocation : allocations) {
			mem::deallocate_sysmem(allocation, alloc_size);
		}

		std::cout << "allocation/deallocation" << std::endl;
		for (int i = 0; i < 16; i++) {
			auto [ptr, status] = mem::allocate_sysmem(alloc_size);
			std::cout << print_range_t{ptr, alloc_size} << std::endl;
			mem::deallocate_sysmem(ptr, alloc_size);
		}

		std::cout << "testing finished" << std::endl;
		std::cout << std::endl;

		return 0;
	}
}

int main(int argc, char* argv[]) {
	if (int status = test_sysmem_info()) {
		return status;
	}
	
	if (int status = test_alloc_free()) {
		return status;
	}
	
	if (int status = test_memory_allocations()) {
		return status;
	}
	
	return 0;
}