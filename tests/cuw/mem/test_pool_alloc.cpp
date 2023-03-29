#include <iomanip>
#include <iostream>

#include <cuw/mem/page_alloc.hpp>
#include <cuw/mem/pool_alloc.hpp>

#include "utils.hpp"
#include "dummy_alloc.hpp"

using namespace cuw;

namespace {
	struct basic_alloc_traits_t {
		static constexpr bool use_resolved_page_size = true;
		static constexpr std::size_t alloc_page_size = block_size_t{1};
		static constexpr std::size_t alloc_block_pool_size = block_size_t{16};
		static constexpr std::size_t alloc_min_block_size = block_size_t{64};

		static constexpr std::size_t alloc_min_pool_power = mem::block_align_pow + 2;
		static constexpr std::size_t alloc_max_pool_power = mem::block_align_pow + 4;
		static constexpr std::size_t alloc_pool_cache_lookups = 4;
		static constexpr std::size_t alloc_raw_cache_lookups = 4;
		static constexpr std::size_t alloc_base_alignment = 16;
		static constexpr bool use_alloc_cache = false;
		static constexpr auto alloc_pool_first_chunk = mem::pool_chunk_size_t::Bytes2;
		static constexpr auto alloc_pool_last_chunk = mem::pool_chunk_size_t::Bytes128;
	};

	using pool_alloc_traits_t = mem::pool_alloc_traits_t<mem::page_alloc_traits_t<basic_alloc_traits_t>>;
	using page_alloc_t = mem::page_alloc_t<dummy_allocator_t<pool_alloc_traits_t>>;
	using pool_alloc_t = mem::pool_alloc_t<page_alloc_t>;

	// some ideas on testing:
	// - we test malloc/realloc
	// - we must definitely test zero allocation
	// - some variants
	//  - malloc(0)
	//  - malloc(0, <any alignment>, 0)
	//  - malloc(<non_zero>)
	//  - malloc(<non_zero>, <any_alignment>, 0)
	//
	//  - realloc(nullptr, 0)
	//  - realloc(<zero_alloc>, <any_size>)
	//  - realloc(<non_zero_alloc>, <any_size>)
	//  - realloc(nullptr, <any_size>, <any_size>, <any_alignment>, 0) -> malloc(<any_size>, <any_alignment>, 0)
	//  - realloc(<zero_alloc>, 0, 0, <any_alignment>, 0)
	//  - realloc(<zero_alloc>, 0, <size > 0>, <any_alignment>, 0)
	//  - realloc(<non_zero_alloc>, <old_size>, <new_size>, <alignment>, 0)
	//
	//  - free(nullptr)
	//  - free(<zero_alloc>)
	//  - free(<zero_alloc>, 0, <any_alignment>, 0)
	//  - free(<non_zero_alloc>) (allocated using both standart & extension API)
	//  - free(<non_zero_alloc>, <size>, <alignment>, 0)
	//
	//  - test pool growth
	int test_pool_alloc() {
		constexpr std::size_t page_size = pool_alloc_t::alloc_page_size;
		constexpr std::size_t basic_alloc_size = page_size << 10;

		pool_alloc_t alloc(basic_alloc_size, page_size);
		void* ptr = alloc.malloc(0);
		alloc.free(ptr);
		ptr = alloc.realloc(ptr, 1);
		ptr = alloc.malloc(15, 16);
		alloc.free(ptr, 15, 16);
		ptr = alloc.malloc(1, 2);
		ptr = alloc.realloc(ptr, 2, 4, 2);

		return 0;
	}

	int test_pool_alloc_random() {
		return 0;
	}	
}

int main() {
	if (test_pool_alloc()) {
		return -1;
	} return 0;
}