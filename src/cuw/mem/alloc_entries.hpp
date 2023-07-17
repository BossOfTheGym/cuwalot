#pragma once

#include <cassert>

#include "core.hpp"
#include "alloc_descr.hpp"
#include "alloc_wrappers.hpp"

namespace cuw::mem {
	struct released_status_t {
		using ad_t = alloc_descr_t;

		ad_t* ad{};
		bool ptr_released{};
	};

	class basic_pool_entry_t : protected alloc_descr_pool_cache_t {
	public:
		using base_t = alloc_descr_pool_cache_t;
		using ad_t = alloc_descr_t;
		using ad_addr_cache_t = alloc_descr_addr_cache_t;

		basic_pool_entry_t(attrs_t _chunk_size_log2 = 0, attrs_t _alignment = 0)
			: chunk_size_log2{_chunk_size_log2}, alignment{_alignment} {}

	private:
		void check_descr(ad_t* descr) {
			assert(descr);
			assert(descr->type == (attrs_t)block_type_t::Pool);
			assert(descr->chunk_size == chunk_size_log2);
		}

	public:
		ad_t* create(void* block, attrs_t offset, attrs_t size, attrs_t capacity, void* data) {
			assert(block);
			assert(data);
			assert(is_aligned(data, alignment));

			ad_t* descr = new (block) ad_t {
				.offset = offset, .size = size,
				.type = (attrs_t)block_type_t::Pool, .chunk_size = chunk_size_log2,
				.capacity = capacity, .head = alloc_descr_head_empty,
				.data = data,
			};

			base_t::insert(descr);
			return descr;
		}

		ad_t* find(const ad_addr_cache_t& addr_cache, void* addr, int max_lookups) {
			if (ad_t* descr = base_t::find(addr, max_lookups)) {
				return descr;
			}
			return addr_cache.find(addr);
		}

		[[nodiscard]] void* acquire() {
			ad_t* descr = base_t::peek();
			if (!descr) {
				return nullptr;
			}

			pool_wrapper_t pool{descr, chunk_size_log2, alignment};
			assert(!pool.full());

			void* chunk = pool.acquire_chunk();
			if (pool.full()) {
				base_t::reinsert_full(descr);
			}

			return chunk;
		}

		[[nodiscard]] released_status_t release(const ad_addr_cache_t& addr_cache, void* ptr, int cache_lookups) {
			if (ad_t* descr = find(addr_cache, ptr, cache_lookups)) {
				return release(ptr, descr);
			}
			return {};
		}

		[[nodiscard]] released_status_t release(void* ptr, ad_t* descr) {
			check_descr(descr);

			pool_wrapper_t pool(descr, chunk_size_log2, alignment);
			assert(!pool.empty());
			
			pool.release_chunk(ptr);
			base_t::reinsert_free(descr);
			return {pool.empty() ? descr : nullptr, true};
		}

		void finish_release(ad_t* descr) {
			check_descr(descr);
			base_t::erase(descr);
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
			check_descr(descr);
			base_t::insert(descr);
		}

		void insert_back(ad_t* descr) {
			check_descr(descr);
			base_t::insert_back(descr);
		}

		void reset() {
			base_t::reset();
		}

		attrs_t get_pool_count() const {
			return base_t::get_pool_count();
		}

		attrs_t get_chunk_size_log2() const {
			return chunk_size_log2;
		}

		attrs_t get_chunk_size() const {
			return value_to_pow2(chunk_size_log2);
		}

		attrs_t get_alignment() const {
			return alignment;
		}
		
	private:
		attrs_t chunk_size_log2{};
		attrs_t alignment{};
	};

	using pool_entry_t = basic_pool_entry_t;

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

	private:
		void check_descr(ad_t* descr) {
			assert(descr);
			assert(descr->type == (attrs_t)block_type_t::Raw);
		}

	public:
		ad_t* find(const ad_addr_cache_t& addr_cache, void* ptr, int max_lookups) {
			if (ad_t* descr = base_t::find(ptr, max_lookups)) {
				return descr;
			}
			
			return addr_cache.find(ptr);
		}

		ad_t* create(void* block, attrs_t offset, attrs_t size, attrs_t alignment, void* data) {
			assert(block);
			assert(data);
			assert(is_aligned(data, alignment));

			ad_t* descr = new (block) ad_t {
				.offset = offset, .size = size, .type = (attrs_t)block_type_t::Raw,
				.chunk_size = value_to_log2(alignment), .data = data
			};
			
			base_t::insert(descr);
			return descr;
		}

		[[nodiscard]] ad_t* release(const ad_addr_cache_t& addr_cache, void* ptr, int max_lookups) {
			return find(addr_cache, ptr, max_lookups);
		}

		[[nodiscard]] ad_t* release(ad_t* descr) {
			return descr;
		}

		void finish_release(ad_t* descr) {
			base_t::erase(descr);
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
			}
			return nullptr;
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

		void reset() {
			base_t::reset();
		}
	};
}