#pragma once

#include <new>
#include <bit>
#include <tuple>
#include <memory>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <type_traits>

#include <cuw/utils/ptr.hpp>
#include <cuw/utils/bst.hpp>
#include <cuw/utils/trb.hpp>
#include <cuw/utils/list.hpp>
#include <cuw/utils/trb_node.hpp>

namespace cuw::mem {
	using ptr_t = std::uintptr_t;
	using attrs_t = std::uint64_t;
	using flags_t = std::uint64_t;
	using block_size_t = std::uint64_t;

	inline constexpr attrs_t min_pool_blocks = 2;
	inline constexpr attrs_t max_pool_blocks = (1 << 16) - 1;
	inline constexpr attrs_t block_pool_head_empty = max_pool_blocks;

	inline constexpr attrs_t min_pool_chunks = 1;
	inline constexpr attrs_t max_pool_chunks = ((attrs_t)1 << 14) - 1;
	inline constexpr attrs_t max_alloc_bits = 48;
	inline constexpr attrs_t max_alloc_size = ((attrs_t)1 << max_alloc_bits) - 1;
	inline constexpr attrs_t alloc_descr_head_empty = max_pool_chunks;

	inline constexpr std::size_t block_align_pow = 6;
	inline constexpr std::size_t block_align = 64;
	inline constexpr std::size_t block_size = 64;

	inline constexpr bool default_use_resolved_page_size = false;
	inline constexpr bool default_use_dirty_optimization_hacks = false; // switch on/off some functionality

	inline constexpr std::size_t default_page_size = 1 << 12; // 4K
	inline constexpr std::size_t default_block_pool_size = 1 << 12; // 4K
	inline constexpr std::size_t default_sysmem_pool_size = 1 << 12; // 4K
	inline constexpr std::size_t default_min_block_size = (std::size_t)1 << 20; // 1M
	inline constexpr std::size_t default_merge_coef = 4;

	inline constexpr attrs_t default_min_pool_power = 15; // 32K
	inline constexpr attrs_t default_max_pool_power = 20; // 1M
	inline constexpr attrs_t default_min_pool_size = (attrs_t)1 << default_min_pool_power;
	inline constexpr attrs_t default_max_pool_size = (attrs_t)1 << default_max_pool_power;

	inline constexpr int default_pool_cache_lookups = 6; // lookups in free_list to access chunk(to realloc or free)
	inline constexpr int default_raw_cache_lookups = 10; // lookups in a list of raw allocations(to realloc or free)

	inline constexpr std::size_t default_base_alignment = 16; // default alignment

	inline constexpr bool default_use_alloc_cache = true; // true, use allocation cache to reduce usage of page_alloc
	inline constexpr bool default_use_locking = true; // true, use locking for multithreading

	inline constexpr std::size_t default_cache_slots = 6; // cache some free blocks for faster allocation
	inline constexpr std::size_t default_min_slot_size = 1 << 15; // 32K as default_min_pool_size
	inline constexpr std::size_t default_max_slot_size = 1 << 20; // 1M as default_min_block_size
	inline constexpr std::size_t default_max_cache_size = default_cache_slots * default_max_slot_size;


	using void_node_traits_t = trb::tree_node_packed_traits_t<void>;

	struct default_tag_t;

	template<class tag_t = default_tag_t>
	using void_node_t = trb::tree_node_packed_t<void_node_traits_t, tag_t>;

	struct addr_index_tag_t;
	using addr_index_t = void_node_t<>;

	struct size_index_tag_t;
	using size_index_t = void_node_t<>;

	using list_entry_t = list::entry_t;

	template<class type_t>
	inline constexpr bool do_fits_block = (sizeof(type_t) == block_size);

	enum block_type_t : attrs_t {
		Pool = 0, // pool of chunks
		Raw, // raw allocation: this is just continious block of memory
	};

	inline constexpr attrs_t chunk_size_empty = ~0;

	// each pool stores chunks aligned to the min(chunk_size, some_max_alignment)
	// max_alignment is usually a page_size 
	// Bytes1 can't be used as a valid chunk_size value but rather as an alignment value by Raw allocations 
	enum class pool_chunk_size_t : attrs_t {
		Empty = chunk_size_empty,
		Min = 0,
		Bytes1 = 0,
		Bytes2,
		Bytes4,
		Bytes8,
		Bytes16,
		Bytes32,
		Bytes64,
		Bytes128,
		Bytes256,
		Bytes512,
		Bytes1024,
		Bytes2048,
		Bytes4096,
		Bytes8192,
		Max = Bytes8192,
	};

	inline constexpr auto default_pool_first_chunk = pool_chunk_size_t::Bytes2;
	inline constexpr auto default_pool_last_chunk = pool_chunk_size_t::Max; 

	template<class int_t>
	constexpr int_t pool_chunk_size(int_t value) {
		return (int_t)1 << (int_t)value;
	}

	template<class int_t>
	constexpr int_t pool_alignment(int_t value) {
		return pool_chunk_size(value);
	}

	template<class int_t>
	constexpr int_t max_pool_chunk_size() {
		return pool_chunk_size<int_t>((int_t)pool_chunk_size_t::Max);
	}

	template<class type_t, class int_t>
	constexpr int_t type_to_pool_chunk() {
		static_assert(std::has_single_bit(sizeof(type_t)));
		return std::countr_zero(sizeof(type_t));
	}

	template<class int_t>
	constexpr int_t value_to_pool_chunk(int_t value) {
		assert(std::has_single_bit(value));
		return std::countr_zero(value);
	}


	inline constexpr attrs_t default_total_raw_bins = 24;


	template<class int_t>
	constexpr bool is_alignment(int_t value) {
		return std::has_single_bit(value);
	}

	template<class int_t>
	constexpr bool is_aligned(int_t value, int_t alignment) {
		assert(is_alignment(alignment)); // must be power of two
		return (value & (alignment - 1)) == 0;
	}

	template<class ptr_t>
	constexpr bool is_aligned(ptr_t* ptr, std::uintptr_t alignment) {
		assert(is_alignment(alignment)); // must be power of two
		return ((std::uintptr_t)ptr & (alignment - 1)) == 0;
	}

	template<class int_t>
	constexpr int_t align_value(int_t value, int_t alignment) {
		assert(is_alignment(alignment)); // must be power of two
		return (value + alignment - 1) & ~(alignment - 1);
	}

	template<class ptr_t>
	constexpr ptr_t* align_value(ptr_t* ptr, std::uintptr_t alignment) {
		return (ptr_t*)align_value((std::uintptr_t)ptr, alignment);
	}

	template <typename enum_t>
	constexpr auto enum_to_value(enum_t e) noexcept {
		return static_cast<std::underlying_type_t<enum_t>>(e);
	}

	// can be used to check if traits struct contains appropriate constexpr field
	template<class option_t, class type_t>
	using enable_option_t = std::enable_if_t<std::is_convertible_v<type_t, option_t>>;

	// simple pool header
	struct pool_hdr_t {
		std::uint16_t next; // not initialized
	};
}