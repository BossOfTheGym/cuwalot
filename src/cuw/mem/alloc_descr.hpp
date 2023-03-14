#pragma once

// NOTE : this file is under construction
// TODO : rename to alloc_descr_t as it is more appropriate name

#include "core.hpp"

namespace cuw::mem {
	using block_descr_list_t = list_entry_t;

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
	struct alignas(block_align) block_descr_t {
		using bd_t = block_descr_t;

		static_assert(sizeof(bd_t) == block_align);

		inline static bd_t* addr_index_to_descr(addr_index_t* addr) {
			return base_to_obj(addr, bd_t, addr_index);
		}

		inline static bd_t* list_entry_to_descr(descr_list_t* list) {
			return base_to_obj(list, bd_t, list_entry);
		}

		// key_ops for tree
		struct addr_ops_t : trb::implicit_key_t<addr_index_t> {
			// overload used for insertion of a new description into the tree
			inline bool compare(addr_index_t* node1, addr_index_t* node2) {
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
			inline bool compare(addr_index_t* node, void* addr) {
				bd_t* descr = addr_index_to_descr(node);
				return (std::uintptr_t)addr >= (std::uintptr_t)descr->block_end();
			}
		};


		inline attrs_t get_offset() const {
			return offset;
		}

		inline attrs_t get_type() const {
			return type;
		}

		inline attrs_t get_size() const {
			return size;
		}

		inline attrs_t get_chunk_size() const {
			return chunk_size;
		}

		inline attrs_t get_offset() const {
			return offset;
		}

		inline void* get_data() const {
			return data;
		}

		// only valid for raw allocation (well, also for pool & aux_pool)
		inline atrts_t get_alignment() const {
			return pool_chunk_size(chunk_size);
		}

		// only valid for raw allocation (well, also for pool)
		inline attrs_t get_true_size() const {
			return align_value(size, get_alignment());
		}


		inline void set_data(void* value) {
			data = value;
		}

		inline void set_size(attrs_t value) {
			size = value;
		}


		inline bool has_addr(void* addr) const {
			auto addr_value = (std::uintptr_t)addr;
			auto data_value = (std::uintptr_t)data;
			return data_value <= addr_value && addr_value < data_value + size;
		}


		inline void* block_start() const {
			return data;
		}

		inline void* block_end() const {
			return (void*)((std::uintptr_t)data + block_size())
		}

		addr_index_t addr_index;
		block_descr_list_t list_entry;
		attrs_t offset:16, size:48;
		attrs_t type:3, chunk_size:5, capacity:14, used:14, count:14, head:14;
		void* data;
	};

	using basic_block_descr_cache_t = list_cache_t<block_descr_list_t>;

	class block_descr_cache_t : protected basic_block_descr_cache_t {
	public:
		using bd_t = block_descr_t;
		using bdl_t = block_descr_list_t;
		using base_t = basic_block_descr_cache_t;

		inline void insert(bd_t* descr) {
			base_t::insert(&descr->list_entry);
		}

		inline void reinsert(bd_t* descr) {
			base_t::reinsert(&descr->list_entry);
		}

		inline void erase(bd_t* descr) {
			base_t::erase(&descr->list_entry);
		}

		inline bd_t* peek() const {
			bdl_t* bdl = base_t::peek();
			if (bdl) {
				return bd_t::list_entry_to_descr(bdl);
			} return nullptr;
		}

		inline bd_t* find(void* addr, int max_lookups) const {
			auto curr = base_t::begin();
			auto head = base_t::end();
			for (int i = 0; curr != head && i < max_lookups; i++, curr++) {
				bd_t* bd = bd_t::list_entry_to_descr(*curr);
				if (bd->has_addr(addr)) {
					return bd;
				}
			} return nullptr;
		}

		inline void adopt(block_descr_cache_t& another, int first_part) {
			base_t part1, part2, part3, part4;
			base_t::split(first_part, part1, part2);
			another.split(first_part, part3, part4);
			part1.adopt(part3);
			part1.adopt(part2);
			part1.adopt(part4);
			static_cast<base_t&>(*this) = std::move(part1);
		}

		template<class func_t>
		void release_all(func_t func) {
			base_t::release_all([&] (bdl_t* bdl) { func(bd_t::list_entry_to_descr(bdl)); });
		}

		void reset() {
			base_t::reset();
		}
	};

	class block_descr_addr_cache_t {
	public:
		using bd_t = block_descr_t;

		inline void insert(bd_t* descr) {
			index = trb::insert_lb(index, &descr->addr_index, bd_t::addr_ops_t{});
		}

		inline void erase(bd_t* descr) {
			index = trb::remove(index, &descr->addr_index, bd_t::addr_ops_t{});
		}

		inline bd_t* find(void* addr) const {
			// lower_bound search can guarantee that addr < block end but it doesn't guarantee that addr belongs to block
			addr_index_t* found = bst::lower_bound(index, bd_t::addr_ops_t{}, addr);
			if (found) {
				bd_t* descr = bd_t::addr_index_to_descr(found);
				return descr->has_addr(addr) ? descr : nullptr;
			} return nullptr;
		}

		inline void adopt(addr_cache_t& another) {
			bst::traverse_inorder(another.index, [&] (addr_index_t* addr) {
				index = trb::insert_lb(index, addr, bd_t::addr_ops_t{});
			});
			another.index = nullptr;
		}

	private:
		addr_index_t* index{};
	};

	template<bool is_pow2>
	class pool_entry_ops_t;

	template<>
	class pool_entry_ops_t<true> {
	public:
		static constexpr bool is_pow2 = true;

		// size, capacity
		std::tuple<attrs_t, attrs_t> get_next_pool_params(attrs_t pools, attrs_t min_pools, attrs max_pools) const {
			pools = std::clamp(pools, min_pools, max_pools);
			attrs_t size = (attrs_t)1 << pools;
			attrs_t capacity = std::min(size >> chunk_size, max_pool_chunks);
			return {capacity << chunk_size, capacity};
		}

		attrs_t get_chunk_size() const {
			return pool_chunk_size(chunk_size);
		}

		attrs_t get_chunk_size_enum() const {
			return chunk_size;
		}

		attrs_t get_max_alignment() const {
			return pool_alignment<attrs_t>(chunk_size);
		}

	private:
		attrs_t chunk_size{};
	};

	template<>
	class pool_entry_ops_t<false> {
	public:
		static constexpr bool is_pow2 = false;

		// size, capacity
		std::tuple<attrs_t, attrs_t> get_next_pool_params(attrs_t pools, attrs_t min_pools, attrs_t max_pools) const {
			pools = std::clamp(pools, min_pools, max_pools);
			attrs_t size = (attrs_t)1 << pools;
			attrs_t capacity = std::min(size / get_chunk_size(), max_pool_chunks);
			return {capacity * get_chunk_size(), capacity};
		}

		attrs_t get_chunk_size() const {
			return aux_pool_chunk_size(chunk_size);
		}

		attrs_t get_chunk_size_enum() const {
			return chunk_size;
		}

		attrs_t get_max_alignemnt() const {
			return aux_pool_alignment<attrs_t>(chunk_size);
		}

	private:
		attrs_t chunk_size{};
	};

	enum class pool_checks_policy_t {
		Default
	};

	template<class __host_t, pool_checks_policy_t __check_policy>
	class pool_checks_t {
	public:
		static constexpr pool_checks_policy_t check_policy = __check_policy;

		using host_t = __host_t;
		using bd_t = typename host_t::block_descr_t;

		bool check_descr(bd_t* descr) const {
			assert(descr);
			host_t& host = static_cast<host_t&>(*this);
			assert(descr->chunk_size == host->get_chunk_size_enum());
			assert(descr->type == host->get_type());
			return true;
		}

		bool check_another(host_t& another) const {
			host_t& host = static_cast<host_t&>(*this);
			assert(host->get_chunk_size_enum() == another->get_chunk_size_enum());
			assert(host->get_type() == another->get_type());
			return true;
		}
	};


	// chunk_size: size of allocated chunk, real value not enum
	// type: block_type_t value, must be pool-like
	// free_cache: list of description blocks (pools) that have free chunks
	// full_cache: list of description blocks (pools) that have no free chunks
	template<bool is_pow2, pool_checks_policy_t check_policy>
	class block_descr_pool_cache_t 
		: public pool_entry_ops_t<is_pow2>
		, public check_descr_t<basic_pool_entry_t, check_policy> {
	public:
		using ops_t = pool_entry_ops_t<is_pow2>;
		using check_t = check_descr_t<basic_pool_entry_t, check_policy>;
		using bd_t = block_descr_t;
		using bd_cache_t = block_descr_cache_t;

		block_descr_pool_cache_t(attrs_t chunk_size, attrs_t _type) : ops_t(chunk_size), type{_type} {}

		std::tuple<attrs_t, attrs_t> get_next_pool_params(attrs_t min_pools, attrs_t max_pools) const {
			return ops_t::get_next_pool_params(pools, min_pools, max_pools);
		}

		void insert_free(bd_t* descr) {
			if (!check_t::check_descr(descr)) {
				return;
			}
			free_pools.insert(descr);
			++pools;
		}

		void reinsert_free(bd_t* descr) {
			if (!check_t::check_descr(descr)) {
				return;
			} free_pools.reinsert(descr);
		}

		void insert_full(bd_t* descr) {
			if (!check_t::check_descr(descr)) {
				return;
			}
			full_pools.insert(descr);
			++pools;
		}

		void reinsert_full(bd_t* descr) {
			if (!check_t::check_descr(descr)) {
				return;
			} full_pools.reinsert(descr);
		}

		void insert_empty(bd_t* descr) {
			if (!check_t::check_descr(descr)) {
				return;
			}
			empty_pools.insert(descr);
			++pools;
		}

		void reinsert_empty(bd_t* descr) {
			if (!check_t::check_descr(descr)) {
				return;
			} empty_pools.reinsert(descr);
		}


		void insert(bd_t* bd) {
			pool_descr_wrapper_t pool{bd};
			if (pool.empty()) {
				insert_empty(bd);
			} if (pool.full()) {
				insert_full(bd);
			} insert_free(bd);
		}		

		void erase(bd_t* bd) {
			if (!check_t::check_descr(bd)) {
				return;
			}
			list_erase(&descr->list_entry); // erase from any list, TODO
			--pools;
		}

		bd_t* peek() const {
			if (bd_t* bd = free_pools.peek(); bd) {
				return bd;
			} if (bd_t* bd = empty_pools.peek(); bd) {
				return bd;
			} return nullptr;
		}

		bd_t* find(void* addr, int max_lookups) const {
			if (bd_t* bd = free_pools.find(addr, max_lookups); bd) {
				return check_t::check_descr(bd) ? bd : nullptr;
			} if (bd_t* bd = full_pools.find(addr, max_lookups); bd) {
				return check_t::check_descr(bd) ? bd : nullptr;
			} return nullptr;
		}

		void adopt(block_descr_pool_cache_t& another, int first_part) {
			if (!check_t::check_another(another)) {
				return;
			}
			free_pools.adopt(another.free_pools, first_part);
			full_pools.adopt(another.full_pools, first_part);
			empty_pools.adopt(another.empty_pools, first_part);
			pools += another.pools;
			another.pools = 0;
		}

		template<class func_t>
		void release_all(func_t func) {
			free_pools.release_all(func);
			full_pools.release_all(func);
			empty_pools.release_all(func);
			pools = 0;
		}

		template<class func_t>
		void release_empty(func_t func) {
			empty_pools.release_all([&] (bd_t* bd) {
				func(bd);
				--pools;
			});
		}

		void reset() {
			free_pools.reset();
			full_pools.reset();
			empty_pools.reset();
		}

		attrs_t get_type() const {
			return type;
		}

	private:
		bd_cache_t free_pools{};
		bd_cache_t full_pools{};
		bd_cache_t empty_pools{};
		attrs_t type{};
		attrs_t pools{};
	};

	enum raw_check_policy_t {
		Default
	};

	template<raw_check_policy_t __check_policy>
	class raw_checks_t {
	public:
		using bd_t = block_descr_t;

		static constexpr raw_check_policy_t check_policy = __check_policy; 

		bool check_descr(bd_t* descr) const {
			assert(descr->get_type() == (attrs_t)block_type_t::Raw);
			return true;
		}
	};

	template<raw_check_policy_t check_policy>
	class block_descr_raw_cache_t
		: public raw_checks_t<check_policy>
		, public block_descr_cache_t {
	public:
		using bd_t = block_descr_t;
		using bd_cache_t = block_descr_cache_t;
		using check_t = raw_checks_t<check_policy>;
		using base_t = block_descr_cache_t;

		void insert(bd_t* bd) {
			if (!check_t::check_descr(bd)) {
				return;
			} base_t::insert(bd);
		}

		void reinsert(bd_t* bd) {
			if (!check_t::check_descr(bd)) {
				return;
			} base_t::reinsert(bd);
		}

		void erase(bd_t* bd) {
			if (!check_t::check_descr(bd)) {
				return;
			} base_t::erase(bd);
		}

		void adopt(block_descr_raw_cache_t& another, int first_part) {
			base_t::adopt(another, first_part);
		}

		template<class func_t>
		void release_all(func_t func) {
			base_t::release_all(func);
		}

		void reset() {
			base_t::reset();
		}
	}
}