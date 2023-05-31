#include <cuw/mem/core.hpp>
#include <cuw/mem/alloc_entries.hpp>

#include "utils.hpp"

using namespace cuw;

using ad_t = mem::alloc_descr_t; 

namespace {
	template<mem::pool_chunk_size_t chunk_enum, std::size_t total_chunks = 4>
	int test_entries_impl() {
		constexpr std::size_t chunk_size = mem::pool_chunk_size((std::size_t)chunk_enum);
		constexpr std::size_t total_pools = 4;
		constexpr std::size_t pool_capacity = total_chunks;
		constexpr std::size_t pool_size = chunk_size * total_chunks;
		constexpr std::size_t total_memory = pool_size * total_pools;
		constexpr std::size_t total_allocations = total_pools * pool_capacity;
		constexpr std::size_t lookups = 2;

		alignas(chunk_size) std::uint8_t data[total_memory] = {};
		alignas(mem::block_align) ad_t descrs[total_pools] = {};
		void* allocated[total_allocations] = {};

		std::size_t curr_descr = 0;

		mem::pool_entry_t entry(chunk_enum, chunk_size);

		auto try_create_empty = [&] () {
			if (curr_descr == total_pools) {
				std::cerr << "no more pools available" << std::endl;
				return false;
			}
			std::size_t offset = curr_descr++;
			ad_t* descr = entry.create(&descrs[offset], offset, pool_size, total_chunks, data + offset * pool_size);
			std::cout << "created descr: " << print_ad_t{descr} << std::endl;
			return true;
		};

		auto allocate = [&] (auto& chunk, int check) {
			chunk = entry.acquire();
			if (!chunk) {
				if (try_create_empty()) {
					chunk = entry.acquire();
					if (!chunk) {
						std::cerr << "something is really wrong..." << std::endl;
						std::abort();
					}
				} else {
					std::cerr << "something went wrong..." << std::endl;
					std::abort();
				}
			}
			std::memset(chunk, check, chunk_size);
			std::cout << "allocated: " << pretty(chunk) << std::endl;
		};

		auto deallocate = [&] (auto& chunk, int check) {
			std::uint8_t chunk_check[chunk_size] = {};
			std::memset(chunk_check, check, chunk_size);
			if (std::memcmp(chunk_check, chunk, chunk_size)) {
				std::cerr << "invalid check value" << std::endl;
				std::abort();
			}
			std::cout << "deallocating: " << pretty(chunk) << std::endl;
			if (ad_t* descr = entry.release({}, chunk, -1)) {
				std::cout << "descr " << print_ad_t{descr} << " is now empty" << std::endl;
			} chunk = nullptr;
		};

		auto check_invalid_allocation = [&] () {
			if (void* chunk = entry.acquire()) {
				std::cerr << "allocation must be invalid!" << std::endl;
				std::abort();
			}
		};

		auto allocate_all = [&] (int start, int end, int step) {
			for (int i = start; i < end; i += step) {
				allocate(allocated[i], i);
			}		
		};

		auto deallocate_all = [&] (int start, int end, int step) {
			for (int i = start; i < end; i += step) {
				deallocate(allocated[i], i);
			}
		};

		std::cout << "testing pool_entry_t " << chunk_size << "..." << std::endl;

		std::cout << "allocating all..." << std::endl;
		allocate_all(0, total_allocations, 1);
		check_invalid_allocation();
		std::cout << "deallocating all..." << std::endl;
		deallocate_all(0, total_allocations, 1);

		std::cout << "allocating all..." << std::endl;
		allocate_all(0, total_allocations, 1);
		check_invalid_allocation();
		std::cout << "deallocating even..." << std::endl;
		deallocate_all(0, total_allocations, 2);
		std::cout << "deallocating uneven..." << std::endl;
		deallocate_all(1, total_allocations, 2);

		std::cout << "allocating all..." << std::endl;
		allocate_all(0, total_allocations, 1);
		check_invalid_allocation();
		std::cout << "deallocate all..." << std::endl;
		deallocate_all(0, total_allocations, 1);

		std::cout << "final data view:" << std::endl << mem_view_t{data, sizeof(data)} << std::endl;
		std::cout << "final descrs view:" << std::endl << mem_view_t{descrs, sizeof(descrs)} << std::endl;

		std::cout << "releasing all..." << std::endl;
		entry.release_all([&] (void* block, std::size_t offset, void* data, std::size_t data_size) {
			std::cout << "releasing descr: " << pretty(block) << " data: " << print_range_t{data, data_size} << std::endl;
			std::memset(block, 0xFF, mem::block_align);
			std::memset(data, 0xFF, data_size);
			return true;
		});

		std::cout << "testing finished." << std::endl << std::endl;

		return 0;
	}

	int test_entries() {
		if (test_entries_impl<mem::pool_chunk_size_t::Bytes2>()) {
			return -1;
		} if (test_entries_impl<mem::pool_chunk_size_t::Bytes4>()) {
			return -1;
		} if (test_entries_impl<mem::pool_chunk_size_t::Bytes8>()) {
			return -1;
		} if (test_entries_impl<mem::pool_chunk_size_t::Bytes16>()) {
			return -1;
		} if (test_entries_impl<mem::pool_chunk_size_t::Bytes32>()) {
			return -1;
		} if (test_entries_impl<mem::pool_chunk_size_t::Bytes64>()) {
			return -1;
		} if (test_entries_impl<mem::pool_chunk_size_t::Bytes128>()) {
			return -1;
		} return 0;
	}
}

int main() {
	if (test_entries()) {
		return -1;
	} return 0;
}