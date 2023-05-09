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
	// x - occupied(allocated) block
	// o - free(unallocated) block
	// - - allocated(used) block
	// + - allocated(managed by fbd) block

	// TODO
	int test_alloc1() {
		std::cout << "testing allocation/deallocation(1)" << std::endl;

		using page_alloc_t = mem::page_alloc_t<basic_alloc_t<1, 4, 2, 10>>;

		page_alloc_t alloc(block_size_t{16}, block_size_t{1});

		// 16 blocks
		// uuuuuuuuuuuuuuuu alloc 10
		// sx----------uuuu dealloc 2
		// sx++--------fxoo dealloc 2
		// sx++--++----fxxo dealloc 2
		// sx++--++--++fxxx dealloc 2
		// sx++++++--++fxxo dealloc 2
		// uuuuuuuuuuuuuuuu

		std::cout << "testing allocation/deallocation finished" << std::endl;

		return 0;
	}

	// TODO
	int test_alloc2() {
		std::cout << "testing allocation/deallocation(2)" << std::endl;

		using page_alloc_t = mem::page_alloc_t<basic_alloc_t<1, 3, 3, 4>>;

		// 32 blocks
		// uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu alloc 6
		// sxoo------uuuuuuuuuuuuuuuuuuuuuu alloc 6
		// sxxo------------uuuuuuuuuuuuuuuu alloc 6
		// sxxx------------------uuuuuuuuuu dealloc 1
		// sxxx+-----------------fxoooooooo dealloc 1
		// sxxx+-+---------------fxxooooooo dealloc 3
		// sxxx+-+-+++-----------fxxxoooooo dealloc 1
		// sxxx+-+-+++-+---------fxxxxooooo dealloc 3
		// sxxx+-+-+++-+-+++-----fxxxxxoooo dealloc 1
		// sxxx+-+-+++-+-+++-+---fxxxxxxooo dealloc 2
		// sxxx+-+-+++-+-+++-+-++fxxxxxxxoo dealloc 1
		// sxxx+++-+++-+-+++-+-++fxxxxxxooo dealloc 1
		// sxxouuuuuu+-+-+++-+-++fxxxxxoooo dealloc 1
		// sxxouuuuuu+++-+++-+-++fxxxxxoooo dealloc 1
		// sxoouuuuuuuuuuuu+-+-++fxxxoooooo dealloc 1
		// sxoouuuuuuuuuuuu+++-++fxxooooooo dealloc 1
		// uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu alloc 6
		// sxoo------uuuuuuuuuuuuuuuuuuuuuu alloc 6
		// sxxo------------uuuuuuuuuuuuuuuu alloc 6
		// sxxx------------------uuuuuuuuuu dealloc 2
		// sxxx++----------------fxoooooooo dealloc 2 
		// sxxx++--------------++fxxooooooo dealloc 2
		// sxxx++--++----------++fxxxoooooo dealloc 2
		// sxxx++--++------++--++fxxxxooooo dealloc 6
		// sxxo++--++uuuuuu++--++fxxxxooooo dealloc 2
		// sxoouuuuuuuuuuuu++--++fxxooooooo dealloc 2
		// uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu alloc 6
		// sxoo------uuuuuuuuuuuuuuuuuuuuuu alloc 6
		// sxxo------------uuuuuuuuuuuuuuuu alloc 6
		// sxxx------------------uuuuuuuuuu dealloc 2
		// sxxx++----------------fxoooooooo dealloc 2 
		// sxxx++--------------++fxxooooooo dealloc 6
		// sxxouuuuuu++--------++fxxooooooo dealloc 6
		// sxxouuuuuu++--++uuuuuufxxooooooo dealloc 1
		// sxxouuuuuu+++-++uuuuuufxoooooooo dealloc 1
		// uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu

		std::cout << "testing allocation/deallocation finished" << std::endl;

		return 0;
	}

	// TODO
	int test_realloc() {
		std::cout << "testing reallocation" << std::endl;

		using page_alloc_t = mem::page_alloc_t<basic_alloc_t<1, 4, 4, 6>>;

		// 20 blocks
		// uuuuuuuuuuuuuuuuuuuu alloc 6
		// sxoo------uuuuuuuuuu dealloc 4
		// sxoo--++++fxoouuuuuu realloc 2,4
		// sxoo----++fxoouuuuuu realloc 4,6
		// sxoo------uuuuuuuuuu dealloc 1
		// sxoo+-----fxoouuuuuu realloc 5,6
		// sxoouuuuuuuuuu------ dealloc 6
		// uuuuuuuuuuuuuuuuuuuu

		std::cout << "testing reallocation finished" << std::endl << std::endl;

		return 0;
	}

	// TODO
	int test_adopt() {
		std::cout << "testing adoption" << std::endl;
		
		using common_alloc_t = basic_alloc_t<1, 3, 3, 4>;
		using seq_alloc_t = scenario_alloc_t<common_alloc_t>;
		using proxy_alloc_t = dummy_alloc_proxy_t<seq_alloc_t>;
		using alloc_t = mem::page_alloc_t<proxy_alloc_t>;

		// 28 blocks
		// ............................
		// uuuuuuuuuuuuuuuuuuuuuuuuuuuu 1. alloc 4
		// 1..1........................
		// sxo----uuuuuuuuuuuuuuuuuuuuu 1. dealloc 2
		// 1..1.1.....1................
		// sxx--++uuuufxouuuuuuuuuuuuuu 1. alloc 4
		// 1..1.1.....1.....1..........
		// sxx--++uuuufxouuu----uuuuuuu 1. dealloc 2
		// 1..1.1.....1.....1.1........
		// sxx--++uuuufxxuuu--++uuuuuuu 2. alloc 4
		// 1..1.1.2...1..2..1.1........
		// sxx--++----fxxsxo--++uuuuuuu 2. alloc 4
		// 1..1.1.2...1..2..1.1.2......
		// sxx--++----fxxsxx--++----uuu 2. dealloc 2
		// 1..1.1.2.2.1..2..1.1.2...2..
		// sxx--++++--fxxsxx--++----fxo 2. dealloc 2
		// 1..1.1.2.2.1..2..1.1.2.2.2..
		// sxx--++++--fxxsxx--++++--fxx
		//
		//          1 adopts 2
		//
		// sxx--++++--fxxsxx--++++--uuu dealloc 2
		// sxx--++uuuufxxsxo--++++--uuu dealloc 2
		// sxx--++uuuufxxuuu--++uuuuuuu dealloc 2
		// sxx--++uuuufxouuuuuuuuuuuuuu dealloc 2
		// uuuuuuuuuuuuuuuuuuuuuuuuuuuu

		std::cout << "adoption test successfully finished" << std::endl << std::endl;

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
	if (test_alloc1()) {
		return -1;
	} if (test_alloc2()) {
		return -1;
	} if (test_realloc()) {
		return -1;
	} if (test_adopt()) {
		return -1;
	} if (test_random_stuff()) {
		return -1;
	} return 0;
}