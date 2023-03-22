#pragma once

#include "core.hpp"
#include "list_cache.hpp"

namespace cuw::mem {
	using block_pool_list_t = list_entry_t;

	// in-memory data structure
	// list_entry(2*64): this is a list entry:)
	// size(48): size of memory block
	// capacity(16): maximum count of possibly allocated blocks
	// used(16) : current capacity
	// count(16) : allocated blocks
	// head(16) : index of the next allocated block
	// reserved(4*64) : reserved, no use
	struct alignas(block_align) block_pool_t {
		using bp_t = block_pool_t;
		using bpl_t = block_pool_list_t;

		inline static bp_t* list_entry_to_block(bpl_t* list) {
			return base_to_obj(list, bp_t, list_entry);
		}

		// offset is zero-based and means offset from the first possible allocated block (not primary block)
		inline static bp_t* primary_block(void* block, attrs_t offset) {
			return transform_ptr<bp_t>(block, -(std::ptrdiff_t)((offset + 1) * block_align));
		}

		inline static attrs_t primary_offset(bp_t* primary_block, void* block) {
			auto primary_block_value = (std::uintptr_t)primary_block;
			auto block_value = (std::uintptr_t)block;
			if (block_value >= primary_block_value) {
				return (block_value - primary_block_value) / block_align;
			} return block_pool_head_empty;
		}

		void* get_data() {
			return this;
		}

		std::size_t get_size() const {
			return size;
		}

		bpl_t list_entry;
		attrs_t niggers:16, size:48;
		attrs_t capacity:16, used:16, count:16, head:16;
		attrs_t reserved[4];
	};

	static_assert(sizeof(block_pool_t) == block_align);

	class block_pool_wrapper_t {
	public:
		using bp_t = block_pool_t;

		inline block_pool_wrapper_t(bp_t* _pool) : pool{_pool} {}
		
	private:
		// index start from 0 from the start of the data segment (not counting first block)
		inline void* get_block(attrs_t index) const {
			return (char*)get_data() + index * block_align;
		}

	public:
		// index is zero-based, zero block is the first block after pool block
		[[nodiscard]] inline std::tuple<void*, attrs_t> acquire() {
			attrs_t index = 0;
			void* block = nullptr;
			if (pool->head != block_pool_head_empty) {
				index = pool->head;
				block = get_block(index);
				pool->count++;
				pool->head = ((pool_hdr_t*)block)->next;
			} else if (pool->used < pool->capacity) {
				index = pool->used++;
				block = get_block(index);
				pool->count++;
			} else {
				return {nullptr, block_pool_head_empty};
			} return {block, index};
		}

		// index is zero-based, zero block is the first block after pool block
		inline void release(void* block, attrs_t index) {
			assert(is_aligned(block, block_align));
			assert(index < pool->capacity);
			((pool_hdr_t*)block)->next = pool->head;
			pool->head = index;
			pool->count--;
		}

		inline void* get_data() const {
			return (char*)pool + block_align;
		}

		inline attrs_t get_size() const {
			return pool->size;
		}

		inline bool empty() const {
			return pool->count == 0;
		} 

		inline bool full() const {
			return pool->count == pool->capacity;
		}

		inline attrs_t get_capacity() const {
			return pool->capacity;
		}

		inline attrs_t get_used() const {
			return pool->used;
		}

		inline attrs_t get_count() const {
			return pool->count;
		}

		inline bp_t* get_pool() const {
			return pool;
		}

	private:
		bp_t* pool{};
	};

	class block_pool_list_cache_t : public list_cache_t<block_pool_list_t> {
	public:
		using bp_t = block_pool_t;
		using bpl_t = block_pool_list_t;
		using base_t = list_cache_t<block_pool_list_t>;

		void insert(bp_t* bp) {
			assert(bp);
			base_t::insert(&bp->list_entry);
		}

		void reinsert(bp_t* bp) {
			assert(bp);
			base_t::reinsert(&bp->list_entry);
		}

		void erase(bp_t* bp) {
			assert(bp);
			base_t::erase(&bp->list_entry);
		}

		bp_t* peek() const {
			if (bpl_t* bpl = base_t::peek()) {
				return bp_t::list_entry_to_block(bpl);
			} return nullptr;
		}

		// bool func(bp_t*)
		template<class func_t>
		void release_all(func_t func) {
			base_t::release_all([&] (bpl_t* bpl) { return func(bp_t::list_entry_to_block(bpl)); });
		}
	};

	class block_pool_cache_t {
	public:
		using bp_t = block_pool_t;
		using bpl_t = block_pool_list_t;
		using bpl_cache_t = block_pool_list_cache_t;

		inline void insert_full(bp_t* pool) {
			full_entries.insert(pool);
		}

		inline void reinsert_full(bp_t* pool) {
			full_entries.reinsert(pool);
		}

		inline void insert_free(bp_t* pool) {
			free_entries.insert(pool);
		}

		inline void reinsert_free(bp_t* pool) {
			free_entries.reinsert(pool);
		}

		inline void insert_empty(bp_t* pool) {
			empty_entries.insert(pool);
		}

		inline void reinsert_empty(bp_t* pool) {
			empty_entries.reinsert(pool);
		}

		inline bp_t* peek_empty() const {
			return empty_entries.peek();
		}


		inline void insert(bp_t* pool) {
			insert_empty(pool);
		}

		inline void erase(bp_t* pool) {
			assert(pool);
			list::erase(&pool->list_entry);
		}

		inline bp_t* peek() const {
			if (bp_t* bp = free_entries.peek()) {
				return bp;
			} if (bp_t* bp = empty_entries.peek()) {
				return bp;
			} return nullptr;
		}

		inline void adopt(block_pool_cache_t& another) {
			full_entries.adopt(another.full_entries);
			free_entries.adopt(another.free_entries);
			empty_entries.adopt(another.empty_entries);
		}

		// bool func(bp_t*)
		template<class func_t>
		void release_all(func_t func) {
			full_entries.release_all(func);
			free_entries.release_all(func);
			empty_entries.release_all(func);
		}

		// bool func(bp_t*)
		template<class func_t>
		void release_empty(func_t func) {
			empty_entries.release_all(func);
		}
		
	private:
		bpl_cache_t full_entries{};
		bpl_cache_t free_entries{};
		bpl_cache_t empty_entries{};
	};

	class block_pool_entry_t : protected block_pool_cache_t {
	public:
		using bp_t = block_pool_t;
		using base_t = block_pool_cache_t;

		inline void create_pool(void* mem, std::size_t size) {
			assert(mem);
			assert(size >= 2 * block_align);
			assert(is_aligned(mem, block_align));

			std::size_t capacity = std::min<std::size_t>(max_pool_blocks, size / block_align - 1);
			bp_t* bp = new (mem) bp_t { .size = size, .capacity = capacity, .head = block_pool_head_empty };
			base_t::insert(bp);
		}

		// returns block_mem, block_offset
		[[nodiscard]] inline std::tuple<void*, attrs_t> acquire() {
			bp_t* bp = base_t::peek();
			if (!bp) {
				return {nullptr, block_pool_head_empty};
			}

			block_pool_wrapper_t pool{bp};
			auto [data, offset] = pool.acquire();
			if (pool.full()) {
				base_t::reinsert_full(bp);
			} return {data, offset};
		}

		// returns pool descriptor(block_pool) when it becomes empty
		inline bp_t* release(void* block_mem, attrs_t block_offset) {
			bp_t* primary_block = bp_t::primary_block(block_mem, block_offset);

			block_pool_wrapper_t pool{primary_block};
			pool.release(block_mem, block_offset);
			if (!pool.empty()) {
				base_t::reinsert_free(primary_block);
				return nullptr; // bp is not empty, return nullptr
			} else {
				base_t::reinsert_empty(primary_block);
				return primary_block; // bp is empty, call finish_release
			}
		}

		// void func(void* mem, std::size_t size)
		template<class func_t>
		void finish_release(bp_t* bp, func_t func) {
			base_t::erase(bp);
			func(bp->get_data(), bp->get_size());
		}

		inline void adopt(block_pool_entry_t& another) {
			base_t::adopt(another);
		}

		// bool func(void* mem, std::size_t size)
		template<class func_t>
		void release_all(func_t func) {
			base_t::release_all([&] (bp_t* bp) { return func(bp->get_data(), bp->get_size()); });
		}

		// bool func(void* mem, std::size_t size)
		template<class func_t>
		void release_empty(func_t func) {
			base_t::release_empty([&] (bp_t* bp) { return func(bp->get_data(), bp->get_size()); });
		}

		// bool func(void*, std::size_t)
		// returns true if empty pool was completely freed
		// returns false if either func doesn't want to free pool or there are no more empty pools 
		template<class func_t>
		bool pop_empty(func_t func) {
			if (bp_t* bp = base_t::peek_empty()) {
				base_t::erase(bp);
				if (func(bp->get_data(), bp->get_size())) {
					return true;
				} base_t::insert_empty(bp);				
			} return false;
		}
	};
}