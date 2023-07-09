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

	int test_alloc1() {
		using page_alloc_t = mem::page_alloc_t<basic_alloc_t<1, 4, 2, 10>>;

		std::cout << "testing allocation/deallocation(1)" << std::endl;

		// 16 blocks
		page_alloc_t alloc(block_size_t{16}, block_size_t{1});

		void* start = alloc.get_alloc_ranges().begin()->ptr;
		void* allocated{};

		// uuuuuuuuuuuuuuuu alloc 10
		allocated = alloc.allocate(block_size_t{10});
		check_allocation(allocated, block_mem_t{start, 2});

		// sx----------uuuu deallocate 2
		alloc.deallocate(block_mem_t{start, 2}, block_size_t{2});

		// sx++--------fxoo deallocate 2
		alloc.deallocate(block_mem_t{start, 6}, block_size_t{2});

		// sx++--++----fxxo deallocate 2
		alloc.deallocate(block_mem_t{start, 10}, block_size_t{2});

		// sx++--++--++fxxx deallocate 2
		alloc.deallocate(block_mem_t{start, 4}, block_size_t{2});

		// sx++++++--++fxxo deallocate 2
		alloc.deallocate(block_mem_t{start, 8}, block_size_t{2});

		// uuuuuuuuuuuuuuuu
		alloc.release_mem(); // nothing must happen

		std::cout << "testing allocation/deallocation finished" << std::endl;

		return 0;
	}

	int test_alloc2() {
		using page_alloc_t = mem::page_alloc_t<basic_alloc_t<1, 10, 4, 4>>;

		std::cout << "testing allocation/deallocation(2)" << std::endl;

		// 32 blocks
		page_alloc_t alloc(block_size_t{32}, block_size_t{1});

		void* start = alloc.get_alloc_ranges().begin()->ptr;
		void* allocated{};

		// uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu alloc 6
		allocated = alloc.allocate(block_size_t{6});
		check_allocation(allocated, block_mem_t{start, 4});

		// sxoo------uuuuuuuuuuuuuuuuuuuuuu alloc 6
		allocated = alloc.allocate(block_size_t{6});
		check_allocation(allocated, block_mem_t{start, 10});

		// sxxo------------uuuuuuuuuuuuuuuu alloc 6
		allocated = alloc.allocate(block_size_t{6});
		check_allocation(allocated, block_mem_t{start, 16});

		// sxxx------------------uuuuuuuuuu deallocate 1
		alloc.deallocate(block_mem_t{start, 4}, block_size_t{1});

		// sxxx+-----------------fxoooooooo deallocate 1
		alloc.deallocate(block_mem_t{start, 6}, block_size_t{1});

		// sxxx+-+---------------fxxooooooo deallocate 3
		alloc.deallocate(block_mem_t{start, 8}, block_size_t{3});

		// sxxx+-+-+++-----------fxxxoooooo deallocate 1
		alloc.deallocate(block_mem_t{start, 12}, block_size_t{1});

		// sxxx+-+-+++-+---------fxxxxooooo deallocate 3
		alloc.deallocate(block_mem_t{start, 14}, block_size_t{3});

		// sxxx+-+-+++-+-+++-----fxxxxxoooo deallocate 1
		alloc.deallocate(block_mem_t{start, 18}, block_size_t{1});

		// sxxx+-+-+++-+-+++-+---fxxxxxxooo deallocate 2
		alloc.deallocate(block_mem_t{start, 20}, block_size_t{2});

		// sxxx+-+-+++-+-+++-+-++fxxxxxxxoo deallocate 1
		alloc.deallocate(block_mem_t{start, 5}, block_size_t{1});

		// sxxx+++-+++-+-+++-+-++fxxxxxxooo deallocate 1
		alloc.deallocate(block_mem_t{start, 7}, block_size_t{1});

		// sxxouuuuuu+-+-+++-+-++fxxxxxoooo deallocate 1
		alloc.deallocate(block_mem_t{start, 11}, block_size_t{1});

		// sxxouuuuuu+++-+++-+-++fxxxxxoooo deallocate 1
		alloc.deallocate(block_mem_t{start, 13}, block_size_t{1});

		// sxoouuuuuuuuuuuu+-+-++fxxxoooooo deallocate 1
		alloc.deallocate(block_mem_t{start, 17}, block_size_t{1});

		// sxoouuuuuuuuuuuu+++-++fxxooooooo deallocate 1
		alloc.deallocate(block_mem_t{start, 19}, block_size_t{1});

		// uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu alloc 6
		allocated = alloc.allocate(block_size_t{6});
		check_allocation(allocated, block_mem_t{start, 4});

		// sxoo------uuuuuuuuuuuuuuuuuuuuuu alloc 6
		allocated = alloc.allocate(block_size_t{6});
		check_allocation(allocated, block_mem_t{start, 10});

		// sxxo------------uuuuuuuuuuuuuuuu alloc 6
		allocated = alloc.allocate(block_size_t{6});
		check_allocation(allocated, block_mem_t{start, 16});

		// sxxx------------------uuuuuuuuuu deallocate 2
		alloc.deallocate(block_mem_t{start, 4}, block_size_t{2});

		// sxxx++----------------fxoooooooo deallocate 2
		alloc.deallocate(block_mem_t{start, 20}, block_size_t{2});

		// sxxx++--------------++fxxooooooo deallocate 2
		alloc.deallocate(block_mem_t{start, 8}, block_size_t{2});

		// sxxx++--++----------++fxxxoooooo deallocate 2
		alloc.deallocate(block_mem_t{start, 16}, block_size_t{2});

		// sxxx++--++------++--++fxxxxooooo deallocate 6
		alloc.deallocate(block_mem_t{start, 10}, block_size_t{6});

		// sxxo++--++uuuuuu++--++fxxxxooooo deallocate 2
		alloc.deallocate(block_mem_t{start, 6}, block_size_t{2});

		// sxoouuuuuuuuuuuu++--++fxxooooooo deallocate 2
		alloc.deallocate(block_mem_t{start, 18}, block_size_t{2});

		// uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu alloc 6
		allocated = alloc.allocate(block_size_t{6});
		check_allocation(allocated, block_mem_t{start, 4});

		// sxoo------uuuuuuuuuuuuuuuuuuuuuu alloc 6
		allocated = alloc.allocate(block_size_t{6});
		check_allocation(allocated, block_mem_t{start, 10});

		// sxxo------------uuuuuuuuuuuuuuuu alloc 6
		allocated = alloc.allocate(block_size_t{6});
		check_allocation(allocated, block_mem_t{start, 16});

		// sxxx------------------uuuuuuuuuu deallocate 2
		alloc.deallocate(block_mem_t{start, 4}, block_size_t{2});

		// sxxx++----------------fxoooooooo deallocate 2 
		alloc.deallocate(block_mem_t{start, 20}, block_size_t{2});

		// sxxx++--------------++fxxooooooo deallocate 6
		alloc.deallocate(block_mem_t{start, 6}, block_size_t{6});

		// sxxouuuuuu++--------++fxxooooooo deallocate 6
		alloc.deallocate(block_mem_t{start, 14}, block_size_t{6});

		// sxxouuuuuu++--++uuuuuufxxooooooo deallocate 1
		alloc.deallocate(block_mem_t{start, 12}, block_size_t{1});

		// sxxouuuuuu+++-++uuuuuufxoooooooo deallocate 1
		alloc.deallocate(block_mem_t{start, 13}, block_size_t{1});

		// uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu
		alloc.release_mem(); // nothing must happen

		std::cout << "testing allocation/deallocation finished" << std::endl;

		return 0;
	}

	int test_realloc() {
		using page_alloc_t = mem::page_alloc_t<basic_alloc_t<1, 4, 4, 6>>;

		std::cout << "testing reallocation" << std::endl;

		// 20 blocks
		page_alloc_t alloc(block_size_t{20}, block_size_t{1});

		void* start = alloc.get_ranges().begin()->ptr;
		void* allocated{};

		// uuuuuuuuuuuuuuuuuuuu alloc 6
		allocated = alloc.allocate(block_size_t{6});
		check_allocation(allocated, block_mem_t{start, 4});

		// sxoo------uuuuuuuuuu deallocate 4
		alloc.deallocate(block_mem_t{start, 6}, block_size_t{4});

		// sxoo--++++fxoouuuuuu realloc 2,4
		allocated = alloc.reallocate(block_mem_t{start, 4}, block_size_t{2}, block_size_t{4});
		check_allocation(allocated, block_mem_t{start, 4});

		// sxoo----++fxoouuuuuu realloc 4,6
		allocated = alloc.reallocate(block_mem_t{start, 4}, block_size_t{4}, block_size_t{6});
		check_allocation(allocated, block_mem_t{start, 4});

		// sxoo------uuuuuuuuuu deallocate 1
		alloc.deallocate(block_mem_t{start, 4}, block_size_t{1});

		// sxoo+-----fxoouuuuuu realloc 5,6
		allocated = alloc.reallocate(block_mem_t{start, 5}, block_size_t{5}, block_size_t{6});

		// sxoouuuuuuuuuu------ deallocate 6
		alloc.deallocate(block_mem_t{start, 14}, block_size_t{6});

		// uuuuuuuuuuuuuuuuuuuu
		alloc.release_mem(); // nothing must happen
		
		std::cout << "testing reallocation finished" << std::endl << std::endl;

		return 0;
	}

	int test_adopt() {
		std::cout << "testing adoption" << std::endl;
		
		using common_alloc_t = basic_alloc_t<1, 3, 3, 4>;
		using seq_alloc_t = scenario_alloc_t<common_alloc_t>;
		using proxy_alloc_t = dummy_alloc_proxy_t<seq_alloc_t>;
		using alloc_t = mem::page_alloc_t<proxy_alloc_t>;

		// 28 blocks
		common_alloc_t common_alloc(block_size_t{28}, block_size_t{1});
		void* start = common_alloc.get_alloc_ranges().begin()->ptr;
		void* allocated{};

		seq_alloc_t seq_alloc(common_alloc, {
			alloc_request_t::allocate(block_size_t{3}, block_mem_t{start, 0}), // 1
			alloc_request_t::allocate(block_size_t{4}, block_mem_t{start, 3}), // 1
			alloc_request_t::allocate(block_size_t{3}, block_mem_t{start, 11}), // 1
			alloc_request_t::allocate(block_size_t{4}, block_mem_t{start, 17}), // 1
			alloc_request_t::allocate(block_size_t{3}, block_mem_t{start, 14}), // 2
			alloc_request_t::allocate(block_size_t{4}, block_mem_t{start, 7}), // 2
			alloc_request_t::allocate(block_size_t{4}, block_mem_t{start, 21}), // 2
			alloc_request_t::allocate(block_size_t{3}, block_mem_t{start, 25}), // 2
		});

		alloc_t alloc1(seq_alloc);
		alloc_t alloc2(seq_alloc);

		// ............................
		// uuuuuuuuuuuuuuuuuuuuuuuuuuuu 1. alloc 4 (allocates 3,4)
		allocated = alloc1.allocate(block_size_t{4});
		check_allocation(allocated, block_mem_t{start, 3});

		// 1..1........................
		// sxo----uuuuuuuuuuuuuuuuuuuuu 1. deallocate 2 (allocates 3)
		alloc1.deallocate(block_mem_t{start, 5}, block_size_t{2});

		// 1..1.1.....1................
		// sxx--++uuuufxouuuuuuuuuuuuuu 1. alloc 4 (allocates 4)
		allocated = alloc1.allocate(block_size_t{4});
		check_allocation(allocated, block_mem_t{start, 17});

		// 1..1.1.....1.....1..........
		// sxx--++uuuufxouuu----uuuuuuu 1. deallocate 2
		alloc1.deallocate(block_mem_t{start, 19}, block_size_t{2});

		// 1..1.1.....1.....1.1........
		// sxx--++uuuufxxuuu--++uuuuuuu 2. alloc 4 (allocates 3,4)
		allocated = alloc2.allocate(block_size_t{4});
		check_allocation(allocated, block_mem_t{start, 7});

		// 1..1.1.2...1..2..1.1........
		// sxx--++----fxxsxo--++uuuuuuu 2. alloc 4 (allocates 4)
		allocated = alloc2.allocate(block_size_t{4});
		check_allocation(allocated, block_mem_t{start, 21});

		// 1..1.1.2...1..2..1.1.2......
		// sxx--++----fxxsxx--++----uuu 2. deallocate 2 (allocates 3)
		alloc2.deallocate(block_mem_t{start, 7}, block_size_t{2});

		// 1..1.1.2.2.1..2..1.1.2...2..
		// sxx--++++--fxxsxx--++----fxo 2. deallocate 2
		alloc2.deallocate(block_mem_t{start, 21}, block_size_t{2});

		// 1..1.1.2.2.1..2..1.1.2.2.2..
		// sxx--++++--fxxsxx--++++--fxx
		alloc1.adopt(alloc2);

		// sxx--++++--fxxsxx--++++--uuu deallocate 2
		alloc1.deallocate(block_mem_t{start, 9}, block_size_t{2});

		// sxx--++uuuufxxsxo--++++--uuu deallocate 2
		alloc1.deallocate(block_mem_t{start, 23}, block_size_t{2});

		// sxx--++uuuufxxuuu--++uuuuuuu deallocate 2
		alloc1.deallocate(block_mem_t{start, 17}, block_size_t{2});

		// sxx--++uuuufxouuuuuuuuuuuuuu deallocate 2
		alloc1.deallocate(block_mem_t{start, 3}, block_size_t{2});

		// uuuuuuuuuuuuuuuuuuuuuuuuuuuu
		alloc1.release_mem(); // nothing must happen
		alloc2.release_mem(); // nothing must happen

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

		constexpr int total_commands = 1 << 14;
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