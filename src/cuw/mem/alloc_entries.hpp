#pragma once

#include <cassert>

#include "core.hpp"
#include "alloc_descr.hpp"
#include "alloc_wrappers.hpp"

namespace cuw::mem {
	template<auto check_policy = pool_checks_policy_t::Default>
	class basic_pool_entry_t : protected alloc_descr_pool_cache_t<check_policy> {
	public:
		using base_t = alloc_descr_pool_cache_t<check_policy>;
		using ad_t = alloc_descr_t;
		using ad_addr_cache_t = alloc_descr_addr_cache_t;

		basic_pool_entry_t(attrs_t chunk_enum, attrs_t type) : base_t(chunk_enum, type) {}

	public:
		attrs_t get_chunk_size() const {
			return base_t::get_chunk_size();
		}

		attrs_t get_chunk_enum() const {
			return base_t::get_chunk_enum();
		}

		// size, capacity
		std::tuple<attrs_t, attrs_t> get_next_pool_params(attrs_t min_pools, attrs_t max_pools) const {
			return base_t::get_next_pool_params(min_pools, max_pools);
		}

		ad_t* find(ad_addr_cache_t& addr_cache, void* addr, int max_lookups) {
			if (ad_t* descr = base_t::find(addr, max_lookups)) {
				return descr;
			} return addr_cache.find(addr);
		}

		// NOTE: data must be aligned beforehand
		ad_t* create(ad_addr_cache_t& addr_cache, void* block, attrs_t offset, attrs_t size, attrs_t capacity, void* data) {
			assert(block);
			assert(data);
			
			ad_t* descr = new (block) ad_t {
				.offset = offset, .size = size,
				.type = base_t::get_type(), .chunk_size = base_t::get_chunk_size_enum(),
				.capacity = capacity, .head = alloc_descr_head_empty,
				.data = data,
			};
			base_t::insert(descr);
			addr_cache.insert(descr);
			return descr;
		}

		template<class wrapper_t>
		[[nodiscard]] void* acquire() {
			ad_t* descr = base_t::peek();
			if (!descr) {
				return nullptr;
			}

			wrapper_t pool(descr, base_t::get_chunk_size_enum());
			assert(!pool.full());

			void* chunk = pool.acquire_chunk();
			if (pool.full()) {
				base_t::reinsert_full(descr);
			} return chunk;
		}

		template<class wrapper_t>
		[[nodiscard]] ad_t* release(ad_addr_cache_t& addr_cache, void* ptr, int cache_lookups) {
			if (ad_t* descr = find(addr_cache, ptr, cache_lookups)) {
				return release<wrapper_t>(ptr, descr);
			} std::abort();
		}

		template<class wrapper_t>
		[[nodiscard]] ad_t* release(void* ptr, ad_t* descr) {
			wrapper_t pool(descr, base_t::get_chunk_size_enum());

			assert(!pool.empty());
			
			pool.release_chunk(ptr);
			base_t::reinsert_free(descr);
			return pool.empty() ? descr : nullptr;
		}

		// void func(void* block, attrs_t offset, void* data, attrs_t size)
		template<class func_t>
		void finish_release(ad_addr_cache_t& addr_cache, ad_t* descr, func_t func) {
			base_t::erase(descr);
			addr_cache.erase(descr);
			func(descr, descr->get_offset(), descr->get_data(), descr->get_size());
		}

		// void func(void* block, attrs_t offset, void* data, attrs_t size)
		template<class func_t>
		void release_all(ad_addr_cache_t& addr_cache, func_t func) {
			base_t::release_all([&] (ad_t* descr) {
				addr_cache.erase(descr);
				func(descr, descr->get_offset(), descr->get_data(), descr->get_size());
			});
		}

		void reset() {
			base_t::reset();
		}
	};

	template<auto check_policy = pool_checks_policy_t::Default>
	class pool_entry_t : public basic_pool_entry_t<check_policy> {
	public:
		using base_t = basic_pool_entry_t<check_policy>;
		using ad_t = alloc_descr_t;
		using ad_addr_cache_t = alloc_descr_addr_cache_t;
		using wrapper_t = basic_pool_wrapper_t;

		pool_entry_t(pool_chunk_size_t chunk_enum = pool_chunk_size_t::Empty)
			: base_t{(attrs_t)chunk_enum, (attrs_t)block_type_t::Pool} {}

		[[nodiscard]] void* acquire() {
			return base_t::template acquire<wrapper_t>();
		}

		[[nodiscard]] ad_t* release(ad_addr_cache_t& addr_cache, void* ptr, int cache_lookups) {
			return base_t::template release<wrapper_t>(addr_cache, ptr, cache_lookups);
		}

		[[nodiscard]] ad_t* release(void* ptr, ad_t* descr) {
			return base_t::template release<wrapper_t>(ptr, descr);
		}
	};

	// raw allocation is made in the following way:
	// 1) descr stores initial size value & alignment
	// 2) when we ask allocator to get some memory we pass aligned size ot it
	// so allocator can guarantee that alignment is also a valid memory
	// 3) ... allocator returns valid memory chunk
	// we can possibly change alignment of a raw
	template<auto check_policy = raw_check_policy_t::Default>
	class raw_entry_t : protected alloc_descr_raw_cache_t<check_policy> {
	public:
		using base_t = alloc_descr_raw_cache_t<check_policy>;
		using ad_t = alloc_descr_t;
		using ad_addr_cache_t = alloc_descr_addr_cache_t;

		ad_t* find(ad_addr_cache_t& addr_cache, void* ptr, int max_lookups) {
			if (ad_t* descr = base_t::find(ptr, max_lookups)) {
				return descr;
			} return addr_cache.find(ptr);
		}

		// NOTE: data must be aligned beforehand
		void create(ad_addr_cache_t& addr_cache,
				void* block, attrs_t offset, attrs_t size, attrs_t alignment, void* data) {
			assert(block);
			assert(data);
			ad_t* descr = new (block) ad_t {
				.offset = offset, .size = align_value(size, alignment), .chunk_size = value_to_pool_chunk(alignment), .data = data
			};
			base_t::insert(descr);
			addr_cache.insert(descr);
		}

		[[nodiscard]] ad_t* release(ad_addr_cache_t& addr_cache, void* ptr, int max_lookups) {
			if (ad_t* descr = find(addr_cache, ptr, max_lookups)) {
				return descr;
			} std::abort();
		}

		[[nodiscard]] ad_t* release(ad_t* descr) {
			return descr;
		}

		// void func(void* block, attrs_t offset, void* data, attrs_t size)
		template<class func_t>
		void finish_release(ad_addr_cache_t& addr_cache, ad_t* descr, func_t func) {
			base_t::erase(descr);
			addr_cache.erase(descr);
			func(descr, descr->get_offset(), descr->get_data(), descr->get_size());
		}

		// void func(void* block, attrs_t offset, void* data, attrs_t size)
		template<class func_t>		
		void release_all(ad_addr_cache_t& addr_cache, func_t func) {
			base_t::release_all([&] (ad_t* descr) {
				addr_cache.erase(descr);
				func(descr, descr->get_offset(), descr->get_data(), descr->get_size());
			});
		}

		[[nodiscard]] ad_t* extract(ad_addr_cache_t& addr_cache, void* ptr, int max_lookups) {
			if (ad_t* descr = find(addr_cache, ptr, max_lookups)) {
				return extract(addr_cache, descr);
			} return nullptr;
		}

		[[nodiscard]] ad_t* extract(ad_addr_cache_t& addr_cache, ad_t* descr) {
			base_t::erase(descr);
			addr_cache.erase(descr);
			return descr;
		}

		void put_back(ad_addr_cache_t& addr_cache, ad_t* descr) {
			base_t::insert(descr);
			addr_cache.insert(descr);
		}

		void adopt(raw_entry_t& another, int first_part) {
			base_t::adopt(another, first_part);
		}

		void reset() {
			base_t::reset();
		}
	};
}