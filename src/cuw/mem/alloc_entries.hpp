#pragma once

#include <cassert>

#include "core.hpp"
#include "alloc_descr.hpp"
#include "alloc_wrappers.hpp"

namespace cuw::mem {
	class basic_pool_entry_t : protected alloc_descr_pool_cache_t {
	public:
		using base_t = alloc_descr_pool_cache_t;
		using ad_t = alloc_descr_t;
		using ad_addr_cache_t = alloc_descr_addr_cache_t;

		basic_pool_entry_t(attrs_t chunk_enum, attrs_t type, attrs_t _alignment)
			: base_t(chunk_enum, type), alignment{_alignment} {}

	public:
		ad_t* create(void* block, attrs_t offset, attrs_t size, attrs_t capacity, void* data) {
			assert(block);
			assert(data);
			assert(is_aligned(data, alignment));

			ad_t* descr = new (block) ad_t {
				.offset = offset, .size = size,
				.type = base_t::get_type(), .chunk_size = base_t::get_chunk_size_enum(),
				.capacity = capacity, .head = alloc_descr_head_empty,
				.data = data,
			};
			base_t::insert(descr);
			return descr;
		}

		ad_t* find(const ad_addr_cache_t& addr_cache, void* addr, int max_lookups) {
			if (ad_t* descr = base_t::find(addr, max_lookups)) {
				return descr;
			} return addr_cache.find(addr);
		}

		template<class wrapper_t>
		[[nodiscard]] void* acquire() {
			ad_t* descr = base_t::peek();
			if (!descr) {
				return nullptr;
			}

			wrapper_t pool(descr, base_t::get_chunk_size_enum(), alignment);
			assert(!pool.full());

			void* chunk = pool.acquire_chunk();
			if (pool.full()) {
				base_t::reinsert_full(descr);
			} return chunk;
		}

		template<class wrapper_t>
		[[nodiscard]] ad_t* release(const ad_addr_cache_t& addr_cache, void* ptr, int cache_lookups) {
			if (ad_t* descr = find(addr_cache, ptr, cache_lookups)) {
				return release<wrapper_t>(ptr, descr);
			} std::abort();
		}

		template<class wrapper_t>
		[[nodiscard]] ad_t* release(void* ptr, ad_t* descr) {
			wrapper_t pool(descr, base_t::get_chunk_size_enum(), alignment);

			assert(!pool.empty());
			
			pool.release_chunk(ptr);
			base_t::reinsert_free(descr);
			return pool.empty() ? descr : nullptr;
		}

		// bool func(void* block, attrs_t offset, void* data, attrs_t size)
		template<class func_t>
		bool finish_release(ad_t* descr, func_t func) {
			if (func(descr, descr->get_offset(), descr->get_data(), descr->get_size())) {
				base_t::erase(descr);
				return true;
			} return false;
		}

		// void func(ad_t* descr)
		template<class func_t>
		void traverse(func_t func) {
			base_t::traverse(func);
		}

		// bool func(void* block, attrs_t offset, void* data, attrs_t size)
		template<class func_t>
		int release_all(func_t func) {
			return base_t::release_all([&] (ad_t* descr) {
				return func(descr, descr->get_offset(), descr->get_data(), descr->get_size());
			});
		}

		void insert(ad_t* descr) {
			base_t::insert(descr);
		}

		void insert_back(ad_t* descr) {
			base_t::insert_back(descr);
		}

		void reset() {
			base_t::reset();
		}

		next_pool_params_t get_next_pool_params(attrs_t min_pools, attrs_t max_pools) const {
			return base_t::get_next_pool_params(min_pools, max_pools);
		}

		attrs_t get_chunk_size() const {
			return base_t::get_chunk_size();
		}

		attrs_t get_chunk_size_enum() const {
			return base_t::get_chunk_size_enum();
		}

		attrs_t get_alignment() const {
			return alignment;
		}
		
	private:
		attrs_t alignment{};
	};

	class pool_entry_t : public basic_pool_entry_t {
	public:
		using base_t = basic_pool_entry_t;
		using ad_t = alloc_descr_t;
		using ad_addr_cache_t = alloc_descr_addr_cache_t;
		using wrapper_t = basic_pool_wrapper_t;

		pool_entry_t(pool_chunk_size_t chunk_enum = pool_chunk_size_t::Empty, attrs_t alignment = 0)
			: base_t{(attrs_t)chunk_enum, (attrs_t)block_type_t::Pool, alignment} {}

		[[nodiscard]] void* acquire() {
			return base_t::template acquire<wrapper_t>();
		}

		[[nodiscard]] ad_t* release(const ad_addr_cache_t& addr_cache, void* ptr, int cache_lookups) {
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
	class raw_entry_t : protected alloc_descr_raw_cache_t {
	public:
		using base_t = alloc_descr_raw_cache_t;
		using ad_t = alloc_descr_t;
		using ad_addr_cache_t = alloc_descr_addr_cache_t;

		ad_t* find(const ad_addr_cache_t& addr_cache, void* ptr, int max_lookups) {
			if (ad_t* descr = base_t::find(ptr, max_lookups)) {
				return descr;
			} return addr_cache.find(ptr);
		}

		ad_t* create(void* block, attrs_t offset, attrs_t size, attrs_t alignment, void* data) {
			assert(block);
			assert(data);
			assert(is_aligned(data, alignment));

			ad_t* descr = new (block) ad_t {
				.offset = offset, .size = size, .type = (attrs_t)block_type_t::Raw,
				.chunk_size = value_to_pool_chunk(alignment), .data = data
			};
			base_t::insert(descr);
			return descr;
		}

		[[nodiscard]] ad_t* release(const ad_addr_cache_t& addr_cache, void* ptr, int max_lookups) {
			if (ad_t* descr = find(addr_cache, ptr, max_lookups)) {
				return descr;
			} std::abort();
		}

		[[nodiscard]] ad_t* release(ad_t* descr) {
			return descr;
		}

		// bool func(void* block, attrs_t offset, void* data, attrs_t size)
		template<class func_t>
		bool finish_release(ad_t* descr, func_t func) {
			if (func(descr, descr->get_offset(), descr->get_data(), descr->get_size())) {
				base_t::erase(descr);
				return true;
			} return false;
		}

		// void func(ad_t* descr)
		template<class func_t>
		void traverse(func_t func) {
			base_t::traverse(func);
		}

		// bool func(void* block, attrs_t offset, void* data, attrs_t size)
		template<class func_t>
		int release_all(func_t func) {
			return base_t::release_all([&] (ad_t* descr) {
				return func(descr, descr->get_offset(), descr->get_data(), descr->get_size());
			});
		}

		[[nodiscard]] ad_t* extract(const ad_addr_cache_t& addr_cache, void* ptr, int max_lookups) {
			if (ad_t* descr = find(addr_cache, ptr, max_lookups)) {
				return extract(descr);
			} return nullptr;
		}

		[[nodiscard]] ad_t* extract(ad_t* descr) {
			base_t::erase(descr);
			return descr;
		}

		void put_back(ad_t* descr) {
			base_t::insert(descr);
		}

		void insert(ad_t* descr) {
			base_t::insert(descr);
		}

		void insert_back(ad_t* descr) {
			base_t::insert_back(descr);
		}

		void adopt(raw_entry_t& another, int first_part) {
			base_t::adopt(another, first_part);
		}

		void reset() {
			base_t::reset();
		}
	};
}