#pragma once

#include "core.hpp"
#include "list_cache.hpp"

namespace cuw::mem {
	using alloc_descr_list_t = list_entry_t;

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
			return base_to_obj(addr, ad_t, addr_index);
		}

		static ad_t* list_entry_to_descr(alloc_descr_list_t* list) {
			return base_to_obj(list, ad_t, list_entry);
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


		attrs_t get_type() const {
			return type;
		}

		attrs_t get_size() const {
			return size;
		}

		attrs_t get_chunk_size() const {
			return chunk_size;
		}

		attrs_t get_offset() const {
			return offset;
		}

		void* get_data() const {
			return data;
		}

		// only valid for raw allocation (well, also for pool)
		attrs_t get_alignment() const {
			return pool_chunk_size(chunk_size);
		}

		// only valid for raw allocation (well, also for pool)
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
			base_t::insert(&descr->list_entry);
		}

		void reinsert(ad_t* descr) {
			base_t::reinsert(&descr->list_entry);
		}

		void erase(ad_t* descr) {
			base_t::erase(&descr->list_entry);
		}

		ad_t* peek() const {
			if (adl_t* adl = base_t::peek()) {
				return ad_t::list_entry_to_descr(adl);
			} return nullptr;
		}

		ad_t* find(void* addr, int max_lookups) {
			auto curr = base_t::begin();
			auto head = base_t::end();
			for (int i = 0; curr != head && i < max_lookups; i++, curr++) {
				if (ad_t* ad = ad_t::list_entry_to_descr(*curr); ad->has_addr(addr)) {
					return ad;
				}
			} return nullptr;
		}

		void adopt(alloc_descr_cache_t& another, int first_part) {
			base_t part1, part2, part3, part4;
			base_t::split(first_part, part1, part2);
			another.split(first_part, part3, part4);
			part1.adopt(part3);
			part1.adopt(part2);
			part1.adopt(part4);
			static_cast<base_t&>(*this) = std::move(part1);
		}

		// void func(ad_t* descr)
		template<class func_t>
		void release_all(func_t func) {
			base_t::release_all([&] (adl_t* entry) {
				func(ad_t::list_entry_to_descr(entry));
				return true;
			});
		}

		void reset() {
			base_t::reset();
		}
	};

	class alloc_descr_addr_cache_t {
	public:
		using ad_t = alloc_descr_t;

		void insert(ad_t* descr) {
			index = trb::insert_lb(index, &descr->addr_index, ad_t::addr_ops_t{});
			++count;
		}

		void erase(ad_t* descr) {
			index = trb::remove(index, &descr->addr_index);
			--count;
		}

		ad_t* find(void* addr) {
			// lower_bound search can guarantee that addr < block end but it doesn't guarantee that addr belongs to block
			if (addr_index_t* found = bst::lower_bound(index, ad_t::addr_ops_t{}, addr)) {
				ad_t* descr = ad_t::addr_index_to_descr(found);
				return descr->has_addr(addr) ? descr : nullptr;
			} return nullptr;
		}

		void adopt(alloc_descr_addr_cache_t& another) {
			addr_index_t* merge_into = index;
			addr_index_t* merge_from = another.index;
			if (count < another.count) {
				std::swap(merge_into, merge_from);
			}
			bst::traverse_inorder(merge_from, [&] (addr_index_t* addr) {
				merge_into = trb::insert_lb(merge_into, addr, ad_t::addr_ops_t{});
			});
			index = merge_into;
			count += another.count;
			another.index = nullptr;
			another.count = 0;
		}

	private:
		addr_index_t* index{};
		std::size_t count{};
	};

	struct next_pool_params_t {
		attrs_t size{};
		attrs_t capacity{};
	};

	class pool_entry_ops_t {
	public:
		pool_entry_ops_t(attrs_t _chunk_enum = chunk_size_empty) : chunk_enum{_chunk_enum} {}

		// constraint resolution order (each constraint can violate previous constraints)
		// 1. pools power
		// 2. pool capacity
		next_pool_params_t get_next_pool_params(attrs_t pools, attrs_t min_pools, attrs_t max_pools) const {
			pools = std::clamp(pools, min_pools, max_pools);
			attrs_t size = (attrs_t)1 << pools;
			attrs_t capacity = std::clamp(size >> chunk_enum, min_pool_chunks, max_pool_chunks);
			return {capacity << chunk_enum, capacity};
		}

		attrs_t get_chunk_size() const {
			return pool_chunk_size(chunk_enum);
		}

		attrs_t get_chunk_size_enum() const {
			return chunk_enum;
		}

		attrs_t get_max_alignment() const {
			return pool_alignment<attrs_t>(chunk_enum);
		}

	private:
		attrs_t chunk_enum{};
	};


	// chunk_size: size of allocated chunk, real value not enum
	// type: block_type_t value, must be pool-like
	// free_cache: list of description blocks (pools) that have free chunks
	// full_cache: list of description blocks (pools) that have no free chunks
	class alloc_descr_pool_cache_t : public pool_entry_ops_t {
	public:
		using ops_t = pool_entry_ops_t;
		using ad_t = alloc_descr_t;
		using ad_cache_t = alloc_descr_cache_t;

		alloc_descr_pool_cache_t(attrs_t chunk_enum, attrs_t _type) : ops_t(chunk_enum), type{_type} {}

	private:
		void check_descr(ad_t* descr) const {
			assert(descr);
			assert(descr->chunk_size == ops_t::get_chunk_size_enum());
			assert(descr->type == get_type());
		}

	public:
		next_pool_params_t get_next_pool_params(attrs_t min_pools, attrs_t max_pools) const {
			return ops_t::get_next_pool_params(pools, min_pools, max_pools);
		}

		void insert_free(ad_t* descr) {
			check_descr(descr);
			free_pools.insert(descr);
			++pools;
		}

		void reinsert_free(ad_t* descr) {
			check_descr(descr);
			free_pools.reinsert(descr);
		}

		void insert_full(ad_t* descr) {
			check_descr(descr);
			full_pools.insert(descr);
			++pools;
		}

		void reinsert_full(ad_t* descr) {
			check_descr(descr);
			full_pools.reinsert(descr);
		}


		void insert(ad_t* descr) {
			check_descr(descr);
			alloc_descr_wrapper_t pool{descr};
			if (pool.full()) {
				insert_full(descr);
			} insert_free(descr);
		}		

		void erase(ad_t* descr) {
			check_descr(descr);
			list::erase(&descr->list_entry);
			--pools;
		}

		ad_t* peek() const {
			return free_pools.peek();
		}

		ad_t* find(void* addr, int max_lookups) {
			if (ad_t* descr = free_pools.find(addr, max_lookups)) {
				return descr;
			} return full_pools.find(addr, max_lookups);
		}

		void adopt(alloc_descr_pool_cache_t& another, int first_part) {
			assert(ops_t::get_chunk_size_enum() == another.get_chunk_size_enum());
			assert(get_type() == another.get_type());

			free_pools.adopt(another.free_pools, first_part);
			full_pools.adopt(another.full_pools, first_part);
			pools += another.pools;
			another.pools = 0;
		}

		// void func(ad_t* descr)
		template<class func_t>
		void release_all(func_t func) {
			free_pools.release_all(func);
			full_pools.release_all(func);
			pools = 0;
		}

		void reset() {
			free_pools.reset();
			full_pools.reset();
		}

		attrs_t get_type() const {
			return type;
		}

	private:
		ad_cache_t free_pools{};
		ad_cache_t full_pools{};
		attrs_t type{};
		attrs_t pools{};
	};


	class alloc_descr_raw_cache_t : public alloc_descr_cache_t {
	public:
		using ad_t = alloc_descr_t;
		using ad_cache_t = alloc_descr_cache_t;
		using base_t = alloc_descr_cache_t;

	private:
		void check_descr(ad_t* descr) const {
			assert(descr);
			assert(descr->get_type() == (attrs_t)block_type_t::Raw);
		}

	public:
		void insert(ad_t* descr) {
			check_descr(descr);
			base_t::insert(descr);
		}

		void reinsert(ad_t* descr) {
			check_descr(descr);
			base_t::reinsert(descr);
		}

		void erase(ad_t* descr) {
			check_descr(descr);
			base_t::erase(descr);
		}

		void adopt(alloc_descr_raw_cache_t& another, int first_part) {
			base_t::adopt(another, first_part);
		}

		// void func(ad_t* descr)
		template<class func_t>
		void release_all(func_t func) {
			base_t::release_all(func);
		}

		void reset() {
			base_t::reset();
		}
	};
}