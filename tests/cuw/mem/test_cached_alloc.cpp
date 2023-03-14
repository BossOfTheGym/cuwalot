#include <bit>
#include <set>
#include <iostream>

#include <cuw/mem/alloc_traits.hpp>
#include <cuw/mem/cached_alloc.hpp>

#include "dummy_alloc.hpp"

using namespace cuw;

namespace {
	struct __test_traits_t {
		static constexpr std::size_t alloc_page_size = 1; // we need it for cached_alloc_traits_t
		static constexpr std::size_t alloc_cache_slots = 4;
		static constexpr std::size_t alloc_min_slot_size = 1;
		static constexpr std::size_t alloc_max_slot_size = 256;

		static_assert(std::has_single_bit(alloc_min_slot_size));
		static_assert(std::has_single_bit(alloc_max_slot_size));
	};
	using test_traits_t = mem::cached_alloc_traits_t<__test_traits_t>;
	using test_cached_alloc_t = mem::cached_alloc_t<dummy_allocator_t<test_traits_t>>;

	struct chunk_t {
		void* ptr{};
		std::size_t size{};
	};

	void populate_cache(test_cached_alloc_t& alloc) {
		constexpr std::size_t max_slot_size = test_cached_alloc_t::alloc_max_slot_size;
		constexpr std::size_t total_slots = test_cached_alloc_t::alloc_cache_slots;

		chunk_t chunks[total_slots];
		for (auto& chunk : chunks) {
			chunk = {alloc.allocate(max_slot_size), max_slot_size};
		} for (auto& chunk : chunks) {
			alloc.deallocate(chunk.ptr, chunk.size);
		}
	}

	void drain_cache(test_cached_alloc_t& alloc) {
		constexpr std::size_t max_pow2 = std::countr_zero(test_cached_alloc_t::alloc_max_slot_size);
		constexpr std::size_t slots = test_cached_alloc_t::alloc_cache_slots;

		chunk_t all_allocs[slots][max_pow2 + 1];
		for (std::size_t i = 0; i < slots; i++) {
			auto& chunks = all_allocs[i];
			for (std::size_t j = 0; j < max_pow2; j++) {
				chunks[j] = {alloc.allocate(1 << j), (std::size_t)1 << j};
				assert(chunks[j].ptr);
			}
			chunks[max_pow2] = {alloc.allocate(1), 1};
			assert(chunks[max_pow2].ptr);
		}

		for (std::size_t chunk = 0; chunk <= max_pow2; chunk++) {
			for (std::size_t slot = 0; slot < slots; slot++) {
				alloc.deallocate(all_allocs[slot][chunk].ptr, all_allocs[slot][chunk].size);
			}
		}
	}

	int test_cached_alloc() {
		test_cached_alloc_t alloc(test_traits_t::alloc_max_cache_size, 1);

		// fill all slots with allocated space, can be part of cached alloc
		populate_cache(alloc);

		// cache is prepopulated, now test how it makes allocations
		drain_cache(alloc);

		// and release all dangling allocated blocks
		alloc.flush_slots();

		return 0;
	}
}

int main(int argc, char* argv[]) {
	return test_cached_alloc();
}