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

		static constexpr std::size_t alloc_min_pool_power = mem::block_align_pow + 2; // 2^8
		static constexpr std::size_t alloc_max_pool_power = mem::block_align_pow + 4; // 2^10
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
	//  - malloc(0, <any alignment>)
	//  - malloc(<non_zero>)
	//  - malloc(<non_zero>, <any_alignment>)
	//
	//  - realloc(nullptr, 0)
	//  - realloc(<zero_alloc>, <any_size>)
	//  - realloc(<non_zero_alloc>, <any_size>)
	//  - realloc(nullptr, <any_size>, <new any_size>, <any_alignment>, 0) -> malloc(<any_size>, <any_alignment>)
	//  - realloc(<zero_alloc>, 0, 0, <any_alignment>)
	//  - realloc(<zero_alloc>, 0, <size > 0>, <any_alignment>)
	//  - realloc(<non_zero_alloc>, <old_size>, <new_size>, <alignment>)
	//
	//  - free(nullptr)
	//
	//  - test pool growth
	int test_pool_alloc() {
		constexpr std::size_t page_size = pool_alloc_t::alloc_page_size;
		constexpr std::size_t basic_alloc_size = page_size << 10;
		constexpr std::size_t min_pool_chunk_size = mem::pool_chunk_size((std::size_t)pool_alloc_t::alloc_pool_first_chunk);
		constexpr std::size_t max_pool_chunk_size = mem::pool_chunk_size((std::size_t)pool_alloc_t::alloc_pool_last_chunk);
		constexpr std::size_t max_pool_alignment = std::min(max_pool_chunk_size, pool_alloc_t::alloc_page_size);

		pool_alloc_t alloc(basic_alloc_size, page_size);

		void* ptr{};
		
		// malloc(0)
		ptr = alloc.malloc(0);
		alloc.free(ptr);

		// malloc(0, <any alignment>)
		for (int i = 0; i <= 4; i++) {
			ptr = alloc.malloc(0, i);
			alloc.free(ptr);
		}

		// malloc(0, <any alignment>) -> free_ext()
		for (int i = 0; i <= 4; i++) {
			ptr = alloc.malloc(0, i);
			alloc.free(ptr, 0, i);
		}

		// malloc(<non_zero>)
		for (int i = 1; i <= max_pool_chunk_size + 1; i++) {
			ptr = alloc.malloc(i);
			alloc.free(ptr);
		}

		// malloc(<non_zero>, <any_alignment>)
		for (int i = 1; i <= max_pool_chunk_size + 1; i++) {
			for (int j = 1; j <= 2 * max_pool_alignment; j *= 2) {
				ptr = alloc.malloc(i, j);
				alloc.free(ptr);
			}
		}

		// malloc(<non_zero>, <any_alignment>)
		for (int i = 1; i <= max_pool_chunk_size + 1; i++) {
			for (int j = 1; j <= 2 * max_pool_alignment; j *= 2) {
				ptr = alloc.malloc(i, j);
				alloc.free(ptr, i, j);
			}
		}

		// realloc(nullptr, 0)
		ptr = alloc.realloc(nullptr, 0);
		alloc.free(ptr);

		// realloc(<zero_alloc>, <any_size>)
		for (int i = 0; i <= max_pool_chunk_size + 1; i++) {
			ptr = alloc.malloc(0);
			ptr = alloc.realloc(ptr, i);
			alloc.free(ptr);
		}

		// realloc(<non_zero_alloc>, <any_size>) (pool-pool,raw scenario)
		for (int i = 1; i <= max_pool_chunk_size + 1; i++) {
			ptr = alloc.malloc(42);
			ptr = alloc.realloc(ptr, i);
			alloc.free(ptr);
		}

		// realloc(<non_zero_alloc>, <any_size>) (raw-pool,raw scenario)
		for (int i = 1; i <= 2 * max_pool_chunk_size; i++) {
			ptr = alloc.malloc(2 * max_pool_chunk_size);
			ptr = alloc.realloc(ptr, i);
			alloc.free(ptr);
		}

		// realloc(nullptr, <any_size>, <any_size>, <any_alignment>, 0) -> malloc(<any_size>, <any_alignment>)
		for (int i = 0; i <= max_pool_chunk_size + 1; i++) {
			for (int j = 1; j <= 2 * max_pool_alignment; j *= 2) {
				ptr = alloc.realloc(nullptr, 42, i, j);
				alloc.free(ptr, i, j);
			}
		}

		// realloc(<zero_alloc>, 0, 0, <any_alignment>)
		for (int i = 1; i <= 2 * max_pool_alignment; i *= 2) {
			ptr = alloc.malloc(0);
			ptr = alloc.realloc(ptr, 0, 0, i);
			alloc.free(ptr, 0, i);
		}

		// realloc(<zero_alloc>, 0, <size > 0>, <any_alignment>)		
		for (int i = 1; i <= 2 * max_pool_chunk_size; i++) {
			for (int j = 1; j <= 2 * max_pool_alignment; j *= 2) {
				ptr = alloc.malloc(0);
				ptr = alloc.realloc(ptr, 0, i, j);
				alloc.free(ptr, i, j);
			}
		}

		// realloc(<non_zero_alloc>, <old_size>, <new_size>, <alignment>)
		for (int i = 1; i <= 2 * max_pool_chunk_size; i++) {
			for (int j = 1; j <= max_pool_alignment; j *= 2) {
				ptr = alloc.malloc(42, j);
				ptr = alloc.realloc(ptr, 42, i, j);
				alloc.free(ptr);
			}
		}

		// free(nullptr)
		alloc.free(nullptr);

		return 0;
	}

	int test_pool_alloc_random() {
		// TODO
		return 0;
	}	
}

int main() {
	if (test_pool_alloc()) {
		return -1;
	} return 0;
}