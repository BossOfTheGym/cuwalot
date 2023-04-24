#include <random>
#include <iomanip>
#include <iostream>

#include <cuw/mem/page_alloc.hpp>
#include <cuw/mem/alloc_traits.hpp>

#include "dummy_alloc.hpp"

using namespace cuw;

namespace {
	template<std::size_t blocks_per_page,
		std::size_t blocks_per_pool,
		std::size_t blocks_per_sysmem_pool,
		std::size_t blocks_per_min_block>
	struct __traits_t {
		static constexpr bool use_resolved_page_size = true;
		static constexpr std::size_t alloc_page_size = block_size_t{blocks_per_page};
		static constexpr std::size_t alloc_block_pool_size = block_size_t{blocks_per_pool};
		static constexpr std::size_t alloc_sysmem_pool_size = block_size_t{blocks_per_sysmem_pool};
		static constexpr std::size_t alloc_min_block_size = block_size_t{blocks_per_min_block};
	};

	template<std::size_t blocks_per_page, std::size_t blocks_per_pool, std::size_t blocks_per_sysmem_pool, std::size_t blocks_per_min_block>
	using traits_t = mem::page_alloc_traits_t<__traits_t<blocks_per_page, blocks_per_pool, blocks_per_sysmem_pool, blocks_per_min_block>>;

	template<std::size_t blocks_per_page, std::size_t blocks_per_pool, std::size_t blocks_per_sysmem_pool, std::size_t blocks_per_min_block>
	using basic_alloc_t = dummy_allocator_t<traits_t<blocks_per_page, blocks_per_pool, blocks_per_sysmem_pool, blocks_per_min_block>>;

	// u - unallocated block
	// s - smd pool
	// f - fbd pool
	// 1 - occupied(allocated) block
	// 0 - free(unallocated) block
	// - - allocated(used) block
	// + - allocated(in allocator) block

	// TODO
	int test_alloc() {
		std::cout << "testing allocation/deallocation" << std::endl;

		using page_alloc_t = mem::page_alloc_t<basic_alloc_t<1, 3, 3, 4>>;

		std::cout << "testing allocation/deallocation finished" << std::endl;

		return 0;
	}

	// TODO
	int test_realloc() {
		std::cout << "testing reallocation" << std::endl;

		using page_alloc_t = mem::page_alloc_t<basic_alloc_t<1, 3, 2, 4>>;

		std::cout << "testing reallocation finished" << std::endl << std::endl;

		return 0;
	}

	// TODO
	int test_adopt() {
		std::cout << "testing adoption" << std::endl;

		using alloc_t = basic_alloc_t<1, 4, 2, 2>;
		using proxy_alloc_t = scenario_alloc_t<alloc_t>;
		using page_alloc_t = mem::page_alloc_t<proxy_alloc_t>;

		std::cout << "adoption test successfully finished" << std::endl << std::endl;

		return 0;
	}

	int test_adopt_advanced() {
		std::cout << "testing adoption(advanced)" << std::endl;
		
		using common_alloc_t = basic_alloc_t<1, 4, 1, 2>;
		using seq_alloc_t = scenario_alloc_t<common_alloc_t>;
		using proxy_alloc_t = dummy_alloc_proxy_t<seq_alloc_t>;
		using alloc_t = mem::page_alloc_t<proxy_alloc_t>;

		std::cout << "adoption test(advanced) successfully finished" << std::endl << std::endl;

		return 0;
	}

	struct allocation_t {
		void* ptr{};
		std::size_t size{};
	};

	std::ostream& operator << (std::ostream& os, const allocation_t& allocation) {
		return os << print_range_t{allocation.ptr, allocation.size};
	}

	int test_random_stuff() {
		std::cout << "testing by random allocations/dellocations..." << std::endl;

		constexpr int total_commands = 1 << 8;
		constexpr std::size_t min_alloc_size = block_size_t{1};
		constexpr std::size_t max_alloc_size = block_size_t{1 << 10};

		using alloc_t = mem::page_alloc_t<basic_alloc_t<1, 8, 4, 32>>;

		alloc_t alloc(block_size_t{1 << 15}, block_size_t{1});
		int_gen_t gen(42);
		std::vector<allocation_t> allocations;

		auto print_status = [&] () {
			std::cout << "-addr_index" << std::endl << addr_index_info_t{alloc};
			std::cout << "-size_index" << std::endl << size_index_info_t{alloc};
			std::cout << "-ranges" << std::endl << ranges_info_t{alloc};
		};

		auto try_allocate = [&] () {
			std::size_t size = gen.gen(min_alloc_size, max_alloc_size);
			std::cout << "trying to allocate block of size " << size << std::endl;
			if (void* ptr = alloc.allocate(size)) {
				std::cout << "success" << std::endl;
				allocations.push_back({ptr, size});
				memset_deadbeef(ptr, size);
			} else {
				std::cout << "failed to allocate block of size " << size << std::endl;
				print_status();
			}
		};

		auto try_deallocate = [&] () {
			if (allocations.empty()) {
				std::cout << "nothing to deallocate" << std::endl;
				return;
			}

			int index = gen.gen(allocations.size());
			std::swap(allocations[index], allocations.back());
			auto curr = allocations.back();
			allocations.pop_back();

			std::cout << "deallocating range " << curr << std::endl;
			std::memset(curr.ptr, 0xFF, curr.size);
			alloc.deallocate(curr.ptr, curr.size);
		};

		auto exec_command = [&] (int num) {
			std::cout << "command No." << num << std::endl;
			int cmd = gen.gen(2);
			if (cmd == 0) {
				try_allocate();
			} else {
				try_deallocate();
			} std::cout << std::endl;
		};

		std::cout << std::endl;
		for (int i = 0; i < total_commands; i++) {
			exec_command(i);
		} std::cout << std::endl;

		std::cout << "deallocating all..." << std::endl;
		std::cout << "total allocations: " << allocations.size() << std::endl;
		for (auto& [ptr, size] : allocations) {
			alloc.deallocate(ptr, size);
		} std::cout << std::endl;

		std::cout << "final status: " << std::endl;
		print_status();

		std::cout << "releasing memory..." << std::endl;
		alloc.release_mem();

		std::cout << "after release:" << std::endl;
		print_status(); 

		std::cout << "testing finished" << std::endl << std::endl;
		
		return 0;
	}
}

int main(int argc, char* argv[]) {
	if (test_alloc()) {
		return -1;
	} if (test_realloc()) {
		return -1;
	} if (test_adopt()) {
		return -1;
	} if (test_adopt_advanced()) {
		return -1;
	} if (test_random_stuff()) {
		return -1;
	} return 0;
}