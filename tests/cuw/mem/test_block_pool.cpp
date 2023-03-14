#include <cstring>
#include <cstdlib>
#include <iostream>
#include <algorithm>

#include <cuw/mem/block_pool.hpp>

using namespace cuw;

namespace {
	// let's hack some stuff
	std::tuple<void*, std::size_t> get_block_alloc(void* primary_block, std::ptrdiff_t index) {
		return {(char*)primary_block + (index + 1) * (std::ptrdiff_t)mem::block_align, index};
	}

	int test_block_pool() {
		constexpr int max_primary_blocks = 4;
		constexpr std::size_t mem_pool_blocks = 63; // 2^n - 1
		constexpr std::size_t mem_pool_size = mem::block_align * (mem_pool_blocks + 1);

		mem::block_pool_entry_t entry;

		void* primary_blocks[max_primary_blocks]{};

		auto print_pool_stats = [&] (auto primary_block) {
			mem::block_pool_wrapper_t pool{(mem::block_pool_t*)primary_block};
			std::cout << " pool: " << primary_block
				<< " list_entry.prev: " << pool.get_pool()->list_entry.prev
				<< " list_entry.next: " << pool.get_pool()->list_entry.next
				<< " size: " << pool.get_size()
				<< " capacity: " << pool.get_capacity()
				<< " used: " << pool.get_used()
				<< " count: " << pool.get_count() << std::endl;
		};

		auto print_stats = [&] () {
			for (auto& primary_block : primary_blocks) {
				if (!primary_block) {
					continue;
				} print_pool_stats(primary_block);
			}
		};

		std::cout << "preallocating primary blocks" << std::endl;
		for (auto& primary_block : primary_blocks) {
			primary_block = std::aligned_alloc(mem::block_align, mem_pool_size);
			std::cout << "primary block allocated: " << primary_block << std::endl;
			entry.create_pool(primary_block, mem_pool_size);
			print_pool_stats(primary_block);
			for (int i = 0; i < mem_pool_blocks; i++) {
				auto [block, offset] = entry.acquire();
				assert(block);
			}
			print_pool_stats(primary_block);
			auto [block, offset] = entry.acquire();
			assert(!block);
		}
		std::cout << std::endl;
		print_stats();

		std::cout << "deallocating blocks on even positions" << std::endl;
		for (auto& primary_block : primary_blocks) {
			for (int i = 0; i < mem_pool_blocks; i += 2) {
				auto [block, offset] = get_block_alloc(primary_block, i);
				entry.release(block, offset);
			}
		} print_stats();

		std::cout << "allocating back blocks on even positions" << std::endl;
		for (auto& primary_block : primary_blocks) {
			for (int i = 0; i < mem_pool_blocks; i += 2) {
				auto [block, offset] = entry.acquire();
			}
		} print_stats();

		std::cout << "deallocating blocks on uneven positions" << std::endl;
		for (auto& primary_block : primary_blocks) {
			for (int i = 1; i < mem_pool_blocks; i += 2) {
				auto [block, offset] = get_block_alloc(primary_block, i);
				entry.release(block, offset);
			}
		} print_stats();

		std::cout << "allocate blocks on uneven positions back" << std::endl;
		for (auto& primary_block : primary_blocks) {
			for (int i = 1; i < mem_pool_blocks; i += 2) {
				auto [block, offset] = entry.acquire();
			}
		} print_stats();

		std::cout << "deallocating all blocks" << std::endl;
		for (auto& primary_block : primary_blocks) {
			for (int i = 0; i < mem_pool_blocks; i++) {
				auto [block, offset] = get_block_alloc(primary_block, i);
				entry.release(block, offset);
			}
		} print_stats();
		
		int blocks_released = 0;
		auto release_func = [&] (void* ptr, std::size_t size, int rem) {
			auto found = std::find(std::begin(primary_blocks), std::end(primary_blocks), ptr);
			assert(found != std::end(primary_blocks));
			if ((found - std::begin(primary_blocks)) % 2 != rem) {
				return false;
			}
			std::cout << "freeing primary_block: " << ptr << std::endl;
			assert(ptr);
			assert(size == mem_pool_size);
			*found = nullptr;
			free(ptr);
			++blocks_released;
			return true;
		};
		std::cout << "releasing all even primary blocks" << std::endl;
		entry.release_all([&] (void* ptr, std::size_t size) { // release even blocks
			return release_func(ptr, size, 0);
		});
		std::cout << "releasing all uneven primary blocks" << std::endl;
		entry.release_all([&] (void* ptr, std::size_t size) { // release uneven blocks
			return release_func(ptr, size, 1);
		});
		assert(blocks_released == max_primary_blocks);

		return 0;
	}
}

int main(int argc, char* argv[]) {
	return test_block_pool();
}