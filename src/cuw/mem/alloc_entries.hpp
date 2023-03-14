#pragma once

// NOTE : this file is under construction

#include <cassert>

#include "core.hpp"
#include "alloc_descr.hpp"
#include "alloc_wrappers.hpp"

namespace cuw::mem {
	template<bool is_pow2, auto check_policy = pool_checks_policy_t::Default>
	class basic_pool_entry_t : protected block_descr_pool_cache_t<is_pow2, check_policy> {
	public:
		using base_t = block_descr_pool_cache_t<is_pow2, check_policy>;
		using bd_t = block_descr_t;
		using bd_addr_cache_t = block_descr_addr_cache_t;

		basic_pool_entry_t(attrs_t chunk_size, attrs_t type) : base_t(chunk_size, type) {}

	public:
		// size, capacity
		std::tuple<attrs_t, attrs_t> get_next_pool_params(attrs_t min_pools, attrs_t max_pools) const {
			return base_t::get_next_pool_params(min_pools, max_pools);
		}

		bd_t* find(const bd_addr_cache_t& addr_cache, void* addr, int max_lookups) {
			if (bd_t* bd = base_t::find(addr_cache, addr, max_lookups); bd) {
				return bd;
			} if (bd_t* bd = addr_cache.find(addr); bd && base_t::check_descr(bd)) {
				return bd;
			} return nullptr;
		}

		void create_empty(bd_addr_cache_t& addr_cache, void* block, attrs_t offset, attrs_t size, attrs_t capacity, void* data) {
			assert(block);
			assert(data);

			bd_t* bd = new (block) bd_t {
				.offset = offset, .size = size,
				.type = base_t::get_type(), .chunk_size = base_t::get_chunk_size_enum(),
				.capacity = capacity, .head = head_empty,
				.data = data,
			};
			base_t::insert_empty(bd);
			addr_cache.insert(bd);
		}

		template<class bd_wrapper_t>
		void* acquire() {
			bd_t* bd = base_t::peek();
			if (!bd) {
				return nullptr;
			}

			bd_wrapper_t pool(bd, base_t::get_chunk_size_enum());
			assert(!pool.full());

			void* chunk = pool.acquire_chunk();
			if (pool.full()) {
				base_t::reinsert_full(bd);
			} return chunk;
		}

		template<class bd_wrapper_t>
		bd_t* release(const bd_addr_cache_t& addr_cache, void* ptr, int cache_lookups) {
			bd_t* bd = find(addr_cache, ptr, cache_lookups);
			if (!bd) {
				abort();
			} return release<bd_wrapper_t>(ptr, bd);
		}

		template<class bd_wrapper_t>
		void release(void* ptr, bd_t* bd) {
			bd_wrapper_t pool(bd, base_t::get_chunk_size_enum());
			assert(!pool.empty());

			pool.release_chunk(ptr);
			if (!pool.empty()) {
				base_t::reinsert_free(bd);
				return nullptr;
			} else {
				base_t::reinsert_empty(bd);
				return bd;
			}
		}

		// void func(void* block, attrs_t offset, void* data, attrs_t size)
		template<class func_t>
		void finish_release(bd_addr_cache_t& addr_cache, bd_t* bd, func_t func) {
			base_t::erase(bd);
			addr_cache.erase(bd);
			func(bd, bd->get_offset(), bd->get_data(), bd->get_size());
		}

		// void func(void* block, attrs_t offset, void* data, attrs_t size)
		template<class func_t>
		void release_all(bd_addr_cache_t& addr_cache, func_t func) {
			base_t::release_all([&] (bd_t* bd) {
				addr_cache.erase(bd);
				func(bd, bd->get_offset(), bd->get_data(), bd->get_size());
			});
		}

		// void func(void* block, attrs_t offset, void* data, attrs_t size)
		template<class func_t>
		void release_empty(bd_addr_cache_t& addr_cache, func_t func) {
			base_t::release_empty([&] (bd_t* bd) {
				addr_cache.erase(bd);
				func(bd, bd->get_offset(), bd->get_data(), bd->get_size());
			});
		}

		void insert(bd_t* bd) {
			base_t::insert(bd);
		}

		void reset() {
			base_t::reset();
		}
	};

	template<bool is_pow2, auto check_policy = pool_check_policy_t::Default>
	class pool_entry_t : public basic_pool_entry_t<is_pow2, check_policy> {
	public:
		using base_t = basic_pool_entry_t<is_pow2, check_policy>;
		using bd_t = typename base_t::bd_t;
		using bd_addr_cache_t = block_descr_addr_cache_t;
		using wrapper_t = basic_pool_wrapper_t<is_pow2>;

		static constexpr block_type_t block_type = is_pow2 ? block_type_t::Pool : block_type_t::PoolAux;

		pool_entry_t(pool_chunk_size_t chunk_size = pool_chunk_size_t::Empty)
			: base_t{(attrs_t)chunk_size, (attrs_t)block_type} {}

		void* acquire() {
			return base_t::acquire<wrapper_t>();
		}

		bd_t* release(const bd_addr_cache_t& addr_cache, void* ptr, int cache_lookups) {
			return base_t::release<wrapper_t>(addr_cache, ptr, cache_lookups);
		}

		bd_t* release(void* ptr, bd_t* bd) {
			return base_t::release<wrapper_t>(ptr, bd);
		}
	};

	template<auto check_policy = pool_check_policy_t::Default>
	class byte_pool_entry_t : public basic_pool_entry_t<true, check_policy> {
	public:
		using base_t = basic_pool_entry_t<true, check_policy>;
		using bd_t = typename base_t::bd_t;
		using bd_addr_cache_t = block_descr_addr_cache_t;
		using wrapper_t = byte_pool_wrapper_t;

		byte_pool_entry_t() : base_t{type_to_pool_chunk<byte_pool_t, attrs_t>(), (attrs_t)block_type_t::PoolBytes} {}

		void* acquire() {
			return base_t::acquire<byte_pool_wrapper_t>();
		}

		bd_t* release(const bd_addr_cache_t& addr_cache, void* ptr, int cache_lookups) {
			base_t::release<byte_pool_wrapper_t>(addr_cache, ptr, cache_lookups);
		}

		bd_t* release(void* ptr, bd_t* bd) {
			base_t::release<byte_pool_wrapper_t>(ptr, bd);
		}
	};

	// raw allocation is made in the following way:
	// 1) descr stores initial size value & alignment
	// 2) when we ask allocator to get some memory we pass aligned size ot it
	// so allocator can guarantee that alignment is also a valid memory
	// 3) ... allocator returns valid memory chunk
	// we can possibly change alignment of a raw
	template<auto check_policy = raw_check_policy_t::Default>
	class raw_entry_t : protected block_descr_raw_cache_t<check_policy> {
	public:
		using base_t = block_descr_raw_cache_t<check_policy>;
		using bd_t = block_descr_t;
		using bd_addr_cache_t = block_descr_addr_cache_t;

		bd_t* find(const bd_addr_cache_t& addr_cache, void* ptr, int max_lookups) {
			if (bd_t* bd = base_t::find(ptr, max_lookups); bd) {
				return bd;
			} if (bd_t* bd = addr_cache.find(ptr); bd && base_t::check_descr(bd)) {
				return bd;
			} return nullptr;
		}

		void create(bd_addr_cache_t& addr_cache,
				void* block, attrs_t offset, attrs_t size, attrs_t alignment, void* data) {
			assert(block);
			assert(data);
			bd_t* bd = new (block) bd_t {
				.offset = offset, .size = size, .chunk_size = value_to_pool_chunk(alignment), .data = data
			};
			base_t::insert_allocated(bd);
			addr_cache.insert(bd);
		}

		bd_t* release(const bd_addr_cache_t& addr_cache, void* ptr, int max_lookups) {
			bd_t* bd = find(addr_cache, ptr, max_lookups);
			if (!bd) {
				abort();
			}
			base_t::reinsert_freed(bd);
			return bd;
		}

		bd_t* release(bd_t* bd) {
			base_t::reinsert_freed(bd);
			return bd;
		}

		// void func(void* block, attrs_t offset, void* data, attrs_t size)
		template<class func_t>
		void finish_release(bd_addr_cache_t& addr_cache, bd_t* bd, func_t func) {
			base_t::erase(bd);
			addr_cache.erase(bd);
			func(bd, bd->get_offset(), bd->get_data(), bd->get_size());
		}

		// void func(void* block, attrs_t offset, void* data, attrs_t size)
		template<class func_t>		
		void release_all(bd_addr_cache_t& addr_cache, func_t func) {
			base_t::release_all([&] (bd_t* bd) {
				addr_cache.erase(bd);
				func(bd, bd->get_offset(), bd->get_data(), bd->get_size());
			});
		}

		// it does not remove found block from addr cache
		bd_t* extract(bd_addr_cache_t& addr_cache, void* ptr, int max_lookups) {
			bd_t* descr = find(addr_cache, ptr, max_lookups);
			if (!descr) {
				abort();
			} return extract(addr_cache, descr);
		}

		// it does not remove found block from addr cache
		bd_t* extract(bd_addr_cache_t& addr_cache, bd_t* bd) {
			base_t::erase(bd);
			addr_cache.erase(bd);
			return bd;
		}

		void put_back(bd_addr_cache_t& addr_cache, bd_t* bd) {
			base_t::insert(bd);
			addr_cache.insert(bd);
		}

		void adopt(raw_entry_t& another, int first_part) {
			base_t::adopt(another, first_part);
		}

		void insert(bd_t* bd) {
			base_t::insert(bd);
		}

		void reset() {
			base_t::reset();
		}
	};
}