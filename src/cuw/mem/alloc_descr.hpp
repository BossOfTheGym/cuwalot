#pragma once

#include "core.hpp"
#include "list_cache.hpp"

namespace cuw::mem {
	using alloc_descr_list_t = list_entry_t;

	// all crucial data fields
	struct alloc_descr_state_t {
		attrs_t offset:16, size:48;
		attrs_t type:3, chunk_size:5, capacity:14, used:14, count:14, head:14;
		void* data;
	};

	// if we want to allocate memory we can use already existing pool and if we don't have one, we allocate one.
	// we have burning desire to place pool storage aligned to page size (welp, we can just align it to the chunk size)
	// so we can deal with alignment issues relatevely cheap and we don't want to waste much space in case of big chunks so... 
	// we have to separate pool description from pool itself. But to store one block description structure would be costly... 
	// so we will pool them too.
	// if allocation is too big to fit into any pool, we will create separate allocation aligned to page size.
	
	// in-memory (not on stack) data structure
	// addr_index: block is added into address index to facilitate address lookup
	// list_entry: blocks are connected into the list to enable cached lookup
	// descr(3): type of the allocated block(enum block_type_t)
	// chunk_size(5): size of the chunk (enum pool_chunk_size_t or enum pool_aux_size_t), can be alignment
	// offset(16): block_pool offset
	// size: size of accessible region (apply page_size alignment to get true size)
	// capacity(14): maximum capacity of the pool, how much chunks it can allocate at max
	// used(14): current capacity
	// count(14): how many chunks were allocated(are in use)
	// head(14): pointer to the next free chunk
	// data: pointer to start of the pool
	struct alignas(block_align) alloc_descr_t {
		using ad_t = alloc_descr_t;

		static ad_t* addr_index_to_descr(addr_index_t* addr) {
			return addr ? base_to_obj(addr, ad_t, addr_index) : nullptr;
		}

		static ad_t* list_entry_to_descr(alloc_descr_list_t* list) {
			return list ? base_to_obj(list, ad_t, list_entry) : nullptr;
		}

		// key_ops for tree
		struct addr_ops_t : trb::implicit_key_t<addr_index_t> {
			// overload used for insertion of a new description into the tree
			bool compare(addr_index_t* node1, addr_index_t* node2) {
				auto addr1 = (std::uintptr_t)addr_index_to_descr(node1)->data;
				auto addr2 = (std::uintptr_t)addr_index_to_descr(node2)->data;
				return addr1 < addr2; // non-intersecting ranges
			}

			// overload used for searching of address to be freed 
			// search will return the first node such that addr < block_end
			// it is possible because all nodes are placed in the following order:
			//  ... <= start_0 < end_0 <= start_1 < end_1 <= start_2 < end_2 <= ...
			// so if addr, for example, belongs to the range [start_2, end_2)
			// this range will be the first item satisfying the binary search in the tree
			// (first range to fail the condition addr >= block_end)
			bool compare(addr_index_t* node, void* addr) {
				ad_t* descr = addr_index_to_descr(node);
				return (std::uintptr_t)addr >= (std::uintptr_t)descr->block_end();
			}
		};


		alloc_descr_state_t get_state() const {
			return {
				.offset = offset, .size = size,
				.type = type, .chunk_size = chunk_size, .capacity = capacity, .used = used, .count = count, .head = head,
				.data = data
			};
		}

		// required for relocation of descr
		void set_state(const alloc_descr_state_t& state) {
			offset = state.offset;
			size = state.size;
			type = state.type;
			chunk_size = state.chunk_size;
			capacity = state.capacity;
			used = state.used;
			count = state.count;
			head = state.head;
			data = state.data;
		}


		attrs_t get_type() const {
			return type;
		}

		attrs_t get_size() const {
			return size;
		}

		attrs_t get_chunk_size() const {
			return chunk_size;
		}

		void set_offset(attrs_t _offset) {
			offset = _offset;
		}

		attrs_t get_offset() const {
			return offset;
		}

		void* get_data() const {
			return data;
		}

		// only valid for raw allocation
		attrs_t get_alignment() const {
			return value_to_pow2(chunk_size);
		}

		// only valid for raw allocation
		attrs_t get_true_size() const {
			return align_value(size, get_alignment());
		}


		void set_data(void* value) {
			data = value;
		}

		void set_size(attrs_t value) {
			size = value;
		}


		bool has_addr(void* addr) const {
			auto addr_value = (std::uintptr_t)addr;
			auto data_value = (std::uintptr_t)data;
			return data_value <= addr_value && addr_value < data_value + size;
		}


		void* block_start() const {
			return data;
		}

		void* block_end() const {
			return (void*)((std::uintptr_t)data + get_size());
		}

		addr_index_t addr_index;
		alloc_descr_list_t list_entry;
		attrs_t offset:16, size:48;
		attrs_t type:3, chunk_size:5, capacity:14, used:14, count:14, head:14;
		void* data;
	}; 

	static_assert(do_fits_block<alloc_descr_t>);

	class alloc_descr_wrapper_t {
	public:
		using ad_t = alloc_descr_t;

		alloc_descr_wrapper_t(ad_t* _descr = nullptr) : descr{_descr} {}

		bool has_addr(void* addr) const {
			return descr->has_addr(addr);
		}

		bool empty() const {
			return descr->count == 0;
		}

		bool full() const {
			return descr->count == descr->capacity;
		}

		bool has_capacity() const {
			return descr->used < descr->capacity;
		}


		attrs_t get_capacity() const {
			return descr->capacity;
		}

		attrs_t get_used() const {
			return descr->used;
		}

		attrs_t get_count() const {
			return descr->count;
		}

		attrs_t get_head() const {
			return descr->head;
		}

		void* get_data() const {
			return descr->data;
		}

		attrs_t get_size() const {
			return descr->size;
		}


		attrs_t inc_used() {
			return descr->used++;
		}

		attrs_t dec_used() {
			return descr->used--;
		}

		attrs_t inc_count() {
			return descr->count++;
		}

		attrs_t dec_count() {
			return descr->count--;
		}

		void set_head(attrs_t value) {
			descr->head = value;
		}

		void set_descr(ad_t* value) {
			descr = value;
		} 

		ad_t* get_descr() const {
			return descr;
		}

	private:
		ad_t* descr{};
	};

	using basic_alloc_descr_cache_t = list_cache_t<alloc_descr_list_t>;

	class alloc_descr_cache_t : protected basic_alloc_descr_cache_t {
	public:
		using ad_t = alloc_descr_t;
		using adl_t = alloc_descr_list_t;
		using base_t = basic_alloc_descr_cache_t;

		void insert(ad_t* descr) {
			assert(descr);
			base_t::insert(&descr->list_entry);
		}

		void insert_back(ad_t* descr) {
			assert(descr);
			base_t::insert_back(&descr->list_entry);
		}

		void reinsert(ad_t* descr) {
			assert(descr);
			base_t::reinsert(&descr->list_entry);
		}

		void erase(ad_t* descr) {
			assert(descr);
			base_t::erase(&descr->list_entry);
		}

		ad_t* peek() const {
			if (adl_t* adl = base_t::peek()) {
				return ad_t::list_entry_to_descr(adl);
			} return nullptr;
		}

		ad_t* find(void* addr, int max_lookups) {
			if (max_lookups == -1) {
				return ad_t::list_entry_to_descr(
					base_t::find([&] (list_entry_t* entry) {
						return ad_t::list_entry_to_descr(entry)->has_addr(addr);
					})
				);
			}

			auto curr = base_t::begin();
			auto last = base_t::end();
			for (int i = 0; curr != last && i < max_lookups; i++, curr++) {
				if (ad_t* ad = ad_t::list_entry_to_descr(*curr); ad->has_addr(addr)) {
					return ad;
				}
			}
			return nullptr;
		}

		// void func(ad_t* descr)
		template<class func_t>
		void traverse(func_t func) {
			base_t::traverse([&] (adl_t* entry) { func(ad_t::list_entry_to_descr(entry)); });
		}

		// bool func(ad_t* descr)
		template<class func_t>
		int release_all(func_t func) {
			return base_t::release_all([&] (adl_t* entry) { return func(ad_t::list_entry_to_descr(entry)); });
		}

		void reset() {
			base_t::reset();
		}
	};

	struct alloc_descr_addr_cache_t {
		using ad_t = alloc_descr_t;

		alloc_descr_addr_cache_t& operator = (alloc_descr_addr_cache_t&& another) noexcept {
			if (this != &another) {
				index = std::exchange(another.index, nullptr);
				count = std::exchange(another.count, 0);
			}
			return *this;
		}

		void insert(ad_t* descr) {
			++count;
			index = trb::insert_lb(index, &descr->addr_index, ad_t::addr_ops_t{});
		}

		void erase(ad_t* descr) {
			--count;
			index = trb::remove(index, &descr->addr_index);
		}

		ad_t* find(void* addr) const {
			// lower_bound search can guarantee that addr < block end but it doesn't guarantee that addr belongs to block
			if (addr_index_t* found = bst::lower_bound(index, addr, ad_t::addr_ops_t{})) {
				ad_t* descr = ad_t::addr_index_to_descr(found);
				return descr->has_addr(addr) ? descr : nullptr;
			}
			return nullptr;
		}

		void reset() {
			index = nullptr;
			count = 0;
		}

		std::size_t get_size() const {
			return count;
		}

		addr_index_t* index{};
		std::size_t count{};
	};

	// chunk_size: size of allocated chunk, real value not enum
	// type: block_type_t value, must be pool-like
	// free_cache: list of description blocks (pool_count) that have free chunks
	// full_cache: list of description blocks (pool_count) that have no free chunks
	class alloc_descr_pool_cache_t {
	public:
		using ad_t = alloc_descr_t;
		using ad_cache_t = alloc_descr_cache_t;

		alloc_descr_pool_cache_t() = default;
		~alloc_descr_pool_cache_t() = default;

		alloc_descr_pool_cache_t(const alloc_descr_pool_cache_t&) = delete;
		alloc_descr_pool_cache_t& operator = (const alloc_descr_pool_cache_t&) = delete;

		alloc_descr_pool_cache_t(alloc_descr_pool_cache_t&&) noexcept = default;
		alloc_descr_pool_cache_t& operator = (alloc_descr_pool_cache_t&&) noexcept = default;

	public:
		void insert_free(ad_t* descr) {
			free_pools.insert(descr);
			++pool_count;
		}

		void reinsert_free(ad_t* descr) {
			free_pools.reinsert(descr);
		}

		void insert_full(ad_t* descr) {
			full_pools.insert(descr);
			++pool_count;
		}

		void reinsert_full(ad_t* descr) {
			full_pools.reinsert(descr);
		}


		void insert(ad_t* descr) {
			++pool_count;
			alloc_descr_wrapper_t pool{descr};
			if (pool.full()) {
				full_pools.insert(descr);
			}
			free_pools.insert(descr);
		}

		void insert_back(ad_t* descr) {
			++pool_count;
			alloc_descr_wrapper_t pool{descr};
			if (pool.full()) {
				full_pools.insert_back(descr);
			}
			free_pools.insert_back(descr);
		}

		void erase(ad_t* descr) {
			list::erase(&descr->list_entry);
			--pool_count;
		}

		ad_t* peek() const {
			return free_pools.peek();
		}

		ad_t* find(void* addr, int max_lookups) {
			if (ad_t* descr = free_pools.find(addr, max_lookups)) {
				return descr;
			}
			return full_pools.find(addr, max_lookups);
		}

		// void func(ad_t* descr)
		template<class func_t>
		void traverse(func_t func) {
			free_pools.traverse(func);
			full_pools.traverse(func);
		}

		// bool func(ad_t* descr)
		template<class func_t>
		int release_all(func_t func) {
			int released = 0;
			released += free_pools.release_all(func);
			released += full_pools.release_all(func);
			pool_count -= released;
			return released;
		}

		void reset() {
			free_pools.reset();
			full_pools.reset();
		}

		attrs_t get_pool_count() const {
			return pool_count;
		}

	private:
		ad_cache_t free_pools{};
		ad_cache_t full_pools{};
		attrs_t pool_count{};
	};


	using alloc_descr_raw_cache_t = alloc_descr_cache_t;
}