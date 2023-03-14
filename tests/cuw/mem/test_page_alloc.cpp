#include <iomanip>
#include <iostream>

#include <cuw/mem/page_alloc.hpp>
#include <cuw/mem/alloc_traits.hpp>

#include "dummy_alloc.hpp"

using namespace cuw;

namespace {
	template<std::size_t blocks_per_pool, std::size_t blocks_per_page, std::size_t blocks_per_min_block>
	struct __traits_t {
		static constexpr bool use_resolved_page_size = true;
		static constexpr std::size_t alloc_block_pool_size = block_size_t{blocks_per_pool};
		static constexpr std::size_t alloc_page_size = block_size_t{blocks_per_page};
		static constexpr std::size_t alloc_min_block_size = block_size_t{blocks_per_min_block};
	};

	template<std::size_t blocks_per_pool, std::size_t blocks_per_page, std::size_t blocks_per_min_block>
	using traits_t = mem::page_alloc_traits_t<__traits_t<blocks_per_pool, blocks_per_page, blocks_per_min_block>>;

	template<std::size_t blocks_per_pool, std::size_t blocks_per_page, std::size_t blocks_per_min_block>
	using basic_alloc_t = dummy_allocator_t<traits_t<blocks_per_pool, blocks_per_page, blocks_per_min_block>>;

	// u - unallocated block
	// f - fbd pool
	// 1 - occupied(allocated) fbd
	// 0 - free(unallocated) fbd
	// - - allocated(used) block
	// + - allocated(in allocator) block

	int test_alloc() {
		std::cout << "testing allocation/deallocation" << std::endl;

		using page_alloc_t = mem::page_alloc_t<basic_alloc_t<3, 1, 4>>;

		page_alloc_t alloc(block_size_t{15}, block_size_t{1});

		// uuuuuuuuuuuuuuu
		void* dummy = nullptr;
		void* start = alloc.get_alloc_ranges().begin()->ptr;
		memset(start, 0x00, block_size_t{15});

		// f10-+++uuuuuuuu
		dummy = alloc.allocate(block_size_t{1});
		memset(dummy, 0x01, block_size_t{1});
		check_allocation(dummy, block_mem_t{start, +3});

		// f10---+uuuuuuuu
		dummy = alloc.allocate(block_size_t{2});
		memset(dummy, 0x02, block_size_t{2});
		check_allocation(dummy, block_mem_t{start, +4});

		// f11-+-+uuuuuuuu
		alloc.deallocate(block_mem_t{start, +4}, block_size_t{1});

		// f11-+-+f10--++u
		dummy = alloc.allocate(block_size_t{2});
		memset(block_mem_t{start, +10}, 0x03, block_size_t{2});
		check_allocation(dummy, block_mem_t{start, +10});

		// f11-+-+f10--++u fails
		dummy = alloc.allocate(block_size_t{3});
		check_allocation(dummy, nullptr);

		// f11-+-+f11+-++u
		alloc.deallocate(block_mem_t{start, +10}, block_size_t{1});

		// f11-+-+f10++++u
		alloc.deallocate(block_mem_t{start, +11}, block_size_t{1});

		// f10++-+f10++++u
		alloc.deallocate(block_mem_t{start, +3}, block_size_t{1});

		// f10++++f10++++u
		alloc.deallocate(block_mem_t{start, +5}, block_size_t{1});

		alloc.release_mem();

		std::cout << "final block: " << std::endl << block_view_t{start, 15} << std::endl;
		std::cout << "testing allocation/deallocation finished" << std::endl;

		return 0;
	}

	int test_realloc() {
		std::cout << "testing reallocation" << std::endl;

		using page_alloc_t = mem::page_alloc_t<basic_alloc_t<3, 1, 4>>;

		page_alloc_t alloc(block_size_t{13}, block_size_t{1});

		// uuuuuuuuuuuuu
		void* dummy{};
		void* start = alloc.get_alloc_ranges().begin()->ptr;
		memset(start, 0x00, block_size_t{13});

		// f10--++uuuuuu
		dummy = alloc.allocate(block_size_t{2});
		memset(dummy, 0x01, block_size_t{2});
		check_allocation(dummy, block_mem_t{start, +3});

		// f00----uuuuuu
		dummy = alloc.reallocate(block_mem_t{start, +3}, block_size_t{2}, block_size_t{4});
		memset(dummy, 0x02, block_size_t{4});
		check_allocation(dummy, block_mem_t{start, +3});

		// f10++++uuuuuu
		alloc.deallocate(block_mem_t{start, +3}, block_size_t{4});

		// f10--++uuuuuu
		dummy = alloc.allocate(block_size_t{2});
		memset(dummy, 0x03, block_size_t{2});
		check_allocation(dummy, block_mem_t{start, +3});

		// f10++++------
		dummy = alloc.reallocate(block_mem_t{start, +3}, block_size_t{2}, block_size_t{6});
		memset(dummy, 0x04, block_size_t{6});
		check_allocation(dummy, block_mem_t{start, +7});

		// f10++++++++++
		alloc.deallocate(block_mem_t{start, +7}, block_size_t{6});

		std::cout << "final state: " << std::endl << block_view_t{start, 13} << std::endl;
		std::cout << "testing reallocation finished" << std::endl;

		return 0;
	}

	template<class fbd_t>
	struct fbd_info_t {
		friend std::ostream& operator << (std::ostream& os, const fbd_info_t& info) {
			return os << " fbd: " << (void*)info.fbd
				<< " off: " << info.fbd->offset
				<< " size: " << info.fbd->size
				<< " data: " << info.fbd->data;
		}

		fbd_t* fbd{};
	};

	template<class page_alloc_t>
	struct addr_index_info_t {
		friend std::ostream& operator << (std::ostream& os, const addr_index_info_t& nodes) {
			for (auto index : nodes.alloc.get_addr_index()) {
				os << fbd_info_t{mem::free_block_descr_t::addr_index_to_descr(index)} << std::endl;
			} return os;
		}

		page_alloc_t& alloc;
	};

	template<class page_alloc_t>
	struct size_index_info_t {
		friend std::ostream& operator << (std::ostream& os, const size_index_info_t& nodes) {
			for (auto index : nodes.alloc.get_size_index()) {
				os << fbd_info_t{mem::free_block_descr_t::size_index_to_descr(index)} << std::endl;
			} return os;
		}

		page_alloc_t& alloc;
	};

	template<class page_alloc_t>
	struct page_alloc_info_t {
		friend std::ostream& operator << (std::ostream& os, const page_alloc_info_t& info) {
			return os << "addr index:" << std::endl << addr_index_info_t{info.alloc} << std::endl
				<< "size index:" << std::endl << size_index_info_t{info.alloc} << std::endl;
		}

		page_alloc_t& alloc;
	};

	int test_adopt() {
		std::cout << "testing adoption" << std::endl;

		using alloc_t = basic_alloc_t<4, 1, 2>;
		using proxy_alloc_t = scenario_alloc_t<alloc_t>;
		using page_alloc_t = mem::page_alloc_t<proxy_alloc_t>;

		// uuuuuuuuuuuuuuuuuuuu
		alloc_t alloc(block_size_t{20}, block_size_t{1});

		void* dummy{};
		void* start = alloc.get_alloc_ranges().begin()->ptr;

		page_alloc_t alloc1(alloc, request_stack_t({
			alloc_request_t::allocate(block_size_t{6}, block_mem_t{start, +0}),
			alloc_request_t::allocate(block_size_t{2}, block_mem_t{start, +8}),
			alloc_request_t::allocate(block_size_t{2}, block_mem_t{start, +16}),
		}));

		// f000--uuuuuuuuuuuuuu
		dummy = alloc1.allocate(block_size_t{2});
		memset(dummy, 0x01, block_size_t{2});
		check_allocation(dummy, block_mem_t{start, +4});

		// f000--uu--uuuuuuuuuu
		dummy = alloc1.allocate(block_size_t{2});
		memset(dummy, 0x02, block_size_t{2});
		check_allocation(dummy, block_mem_t{start, +8});

		// f000--uu--uuuuuu--uu
		dummy = alloc1.allocate(block_size_t{2});
		memset(dummy, 0x03, block_size_t{2});
		check_allocation(dummy, block_mem_t{start, +16});

		// f100++uu--uuuuuu--uu
		alloc1.deallocate(block_mem_t{start, +4}, block_size_t{2});
		// f110++uu++uuuuuu--uu
		alloc1.deallocate(block_mem_t{start, +8}, block_size_t{2});
		// f111++uu++uuuuuu++uu
		alloc1.deallocate(block_mem_t{start, +16}, block_size_t{2});

		page_alloc_t alloc2(alloc, request_stack_t({
			alloc_request_t::allocate(block_size_t{6}, block_mem_t{start, +10}),
			alloc_request_t::allocate(block_size_t{2}, block_mem_t{start, +18}),
			alloc_request_t::allocate(block_size_t{2}, block_mem_t{start, +6}),
		}));

		// f111++uu++f000--++uu
		dummy = alloc2.allocate(block_size_t{2});
		memset(dummy, 0x04, block_size_t{2});
		check_allocation(dummy, block_mem_t{start, +14});

		// f111++uu++f000--++--
		dummy = alloc2.allocate(block_size_t{2});
		memset(dummy, 0x05, block_size_t{2});
		check_allocation(dummy, block_mem_t{start, +18});

		// f111++--++f000--++--
		dummy = alloc2.allocate(block_size_t{2});
		memset(dummy, 0x06, block_size_t{2});
		check_allocation(dummy, block_mem_t{start, +6});

		// f111++--++f100++++--
		alloc2.deallocate(block_mem_t{start, +14}, block_size_t{2});
		// f111++--++f110++++++
		alloc2.deallocate(block_mem_t{start, +18}, block_size_t{2});
		// f111++++++f111++++++
		alloc2.deallocate(block_mem_t{start, +6}, block_size_t{2});

		std::cout << "alloc.ranges" << std::endl << ranges_info_t{alloc} << std::endl;
		std::cout << "alloc.alloc_ranges" << std::endl << alloc_ranges_info_t{alloc} << std::endl;
		std::cout << "alloc1" << std::endl << page_alloc_info_t{alloc1} << std::endl;
		std::cout << "alloc2" << std::endl << page_alloc_info_t{alloc2} << std::endl;

		alloc1.adopt(alloc2);
		alloc1.release_mem();

		std::cout << "final state:" << std::endl << block_view_t{start, 20} << std::endl;
		std::cout << "adoption test successfully finished" << std::endl;

		return 0;
	}

	int test_adopt_advanced() {
		// std::cout << "testing adoption(advanced)" << std::endl;

		// using alloc_t = scenario_alloc_t<basic_alloc_t<4, 1, 2>>;
		// using proxy_alloc_t = dummy_alloc_proxy_t<basic_scenario_alloc_t>;
		// using page_alloc_t = mem::page_alloc_t<proxy_alloc_t>;

		// // TODO

		// std::cout << "adoption test successfully finished" << std::endl;

		return 0;
	}

	int test_random_stuff() {
		// TODO : series of random allocations or deallocations
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
	} if (test_random_stuff()) {
		return -1;
	} return 0;
}