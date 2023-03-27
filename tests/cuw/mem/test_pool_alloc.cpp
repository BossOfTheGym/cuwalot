#include <iomanip>
#include <iostream>

#include <cuw/mem/pool_alloc.hpp>

#include "utils.hpp"
#include "dummy_alloc.hpp"

using namespace cuw;

namespace {
	struct __traits_t {
		static constexpr bool use_resolved_page_size = true;
		static constexpr std::size_t alloc_page_size = block_size_t{1};
		static constexpr std::size_t alloc_block_pool_size = block_size_t{16};
		static constexpr std::size_t alloc_min_block_size = block_size_t{64};

		//static constexpr std::size_t alloc_cache_slots = 0; // TODO
		//static constexpr std::size_t alloc_min_slot_size = 0; // TODO
		//static constexpr std::size_t alloc_max_slot_size = 0; // TODO

		static constexpr std::size_t alloc_min_pool_power = 1;
		static constexpr std::size_t alloc_max_pool_power = 1;
		static constexpr std::size_t alloc_min_pool_size = block_size_t{8};
		static constexpr std::size_t alloc_max_pool_size = block_size_t{8};
		static constexpr std::size_t alloc_pool_cache_lookups = 4;
		static constexpr std::size_t alloc_raw_cache_lookups = 4;
		static constexpr std::size_t alloc_base_alignment = 16;
		static constexpr bool use_alloc_cache = false;
		static constexpr auto alloc_pool_first_chunk = mem::pool_chunk_size_t::Bytes2;
		static constexpr auto alloc_pool_last_chunk = mem::pool_chunk_size_t::Bytes128;
	};

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
	int test_pool_alloc() {
		return 0;
	}

	int test_pool_alloc_random() {
		return 0;
	}	
}

struct base_t {
	static constexpr int a = 0;
	static constexpr int b = 0;
	static constexpr int c = 0;
};

template<class traits_t>
struct traits1_t : traits_t {
private:
	static constexpr int _b = 1;
	static constexpr int _c = 1;
public:
	static constexpr int a = 1;
};

template<class traits_t>
struct traits2_t : traits_t {
private:
	static constexpr int _a = 2;
	static constexpr int _c = 2;
public:
	static constexpr int b = 2;
};

template<class traits_t>
struct traits3_t : traits_t {
private:
	static constexpr int _a = 3;
	static constexpr int _b = 3;
public:
	static constexpr int c = 3;
};

using hui_t = traits3_t<traits2_t<traits1_t<base_t>>>;

int main() {
	std::cout << hui_t::a << " " << hui_t::b << " " << hui_t::c << std::endl; 
	return 0;
}