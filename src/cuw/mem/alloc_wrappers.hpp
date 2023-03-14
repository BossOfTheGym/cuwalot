#pragma once

#include "cumalot_core.hpp"
#include "cumalot_block_descr.hpp"

namespace cbt::mem {
	// if we want to allocate memory we can use already existing pool and if we don't have one, we allocate one.
	// we have burning desire to place pool storage aligned to page size (welp, we can just align it to the chunk size)
	// so we can deal with alignment issues relatevely cheap and we don't want to waste much space in case of big chunks so... 
	// we have to separate pool description from pool itself. But to store one block description structure would be costly... 
	// so we will pool them too.
	// if allocation is too big to fit into any pool, we will create separate allocation aligned to page size.
	//
	// now several words about pools:
	// size = 1, alignment = 1 => pool of pools ... of byte pools
	// size = 2,4,.., alignment = 2,4,.. => ordinary pool
	// every pool will use 14-bit head that will serve as an index of a chunk
	// for single allocation pool_chunk_size becomes alignment
	// each pool can store not more than 2^14 - 1 chunks

	// 16 bytes
	// we continiously peek chunk from the main pool until it's exausted (consider it free) then we acquire (allocate) it
	// if we free some chunk from byte pool then pool is returned into the main pool and considered free again  
	// next: pointer to the next small pool
	// mask: mask where set bit means that appropriate chunk is allocated
	// last 4 most significant bits are not used(occupied by next and mask themselves)
	// this is little bit controversial but this pool stores chunks of 16 bytes so it make 12 times more allocations
	// than ordinary pool
	// chunks : 1 byte memory to allocate
	class byte_pool_t {
	public:
		static inline constexpr std::size_t hdr_size = 4;
		static inline constexpr std::size_t max_chunks = 12;

		inline std::uint16_t get_chunk_index(void* chunk) {
			return (std::uint16_t)((uint8_t*)chunk - &chunks[0]);
		}

		inline void* acquire_chunk() {
			int index = std::countr_one(mask);
			if (index < max_chunks) {
				mask |= 1 << index;
				return &chunks[index];
			} return nullptr;
		}

		inline void* peek_chunk() {
			int index = std::countr_one(mask);
			if (index < max_chunks) {
				return &chunks[index];
			} return nullptr;
		}

		inline void release_chunk(void* chunk) {
			assert(has_addr(chunk));
			mask &= ~((std::uint16_t)1 << get_chunk_index(chunk));
		}

		inline bool has_addr(void* addr) const {
			auto data_value = (std::uintptr_t)(&chunks[0]);
			auto addr_value = (std::uintptr_t)addr;
			return data_value <= addr_value && addr_value < data_value + max_chunks;
		}

		inline bool has_chunk(void* chunk) const {
			return has_addr(chunk);
		}

		inline bool empty() const {
			return mask == 0x0000;
		}

		inline bool full() const {
			return mask == 0x3FFF;
		}

	private:
		std::uint16_t next;
		std::uint16_t mask;
		std::uint8_t chunks[max_chunks];
	};

	class pool_descr_wrapper_t {
	public:
		using bd_t = block_descr_t;

		pool_descr_wrapper_t(bd_t* wrapped = nullptr) : descr{wrapped} {}

		inline bool has_addr(void* addr) const {
			return descr->has_add(addr);
		}

		inline bool empty() const {
			return descr->count == 0;
		}

		inline bool full() const {
			return descr->count == descr->capacity;
		}

		inline bool has_capacity() const {
			return descr->used < descr->capacity;
		}


		inline attrs_t get_capacity() const {
			return descr->capacity;
		}

		inline attrs_t get_used() const {
			return descr->used;
		}

		inline atrts_t get_count() const {
			return descr->count;
		}

		inline attrs_t get_head() const {
			return descr->head;
		}

		inline void* get_data() const {
			return descr->data;
		}

		inline attrs_t get_size() const {
			return descr->size;
		}


		inline attrs_t inc_used() {
			return descr->used++;
		}

		inline attrs_t dec_used() {
			return descr->used--;
		}

		inline attrs_t inc_count() {
			return descr->count++;
		}

		inline attrs_t dec_count() {
			return descr->count--;
		}

		inline void set_head(attrs_t value) {
			descr->head = value;
		}

		inline void set_descr(block_descr_t* value) {
			descr = value;
		} 

		inline block_descr_t* get_descr() const {
			return descr;
		}

	private:
		block_descr_t* descr{};
	};

	template<bool __is_pow2>
	class pool_ops_t;

	template<>
	class pool_ops_t<true> : pool_descr_wrapper_t {
	public:
		using base_t = pool_descr_wrapper_t;
		using bd_t = typename base_t::bd_t;

		static constexpr bool is_pow2 = true;

		pool_ops_t(bd_t* descr = nullptr, attrs_t chunk_size_ = chunk_size_empty)
			: base_t(descr), chunk_size{chunk_size_} {}

		attrs_t get_chunk_size() const {
			return pool_chunk_size<attrs_t>(chunk_size);
		}

		attrs_t get_chunk_size_enum() const {
			return chunk_size;
		}

		void* get_chunk_memory(attrs_t index) const {
			return (char*)base_t::get_data() + ((std::uintptr_t)index << chunk_size);
		}

		attrs_t get_chunk_index(void* chunk) const {
			assert(has_chunk(chunk));
			auto diff = (std::uintptr_t)chunk - (std::uintptr_t)base_t::get_data();
			return (attrs_t)(diff >> (std::uintptr_t)chunk_size);
		}

		void* refine_chunk_memory(void* chunk) const {
			auto chunk_value = (std::uintptr_t)chunk;
			auto data_value = (std::uintptr_t)base_t::get_data();
			if (chunk_value < data_value) {
				return nullptr;
			} return (void*)(chunk_value & ((std::uintptr_t)chunk_size - 1));
		}

		bool has_chunk(void* addr) const {
			auto addr_value = (std::uintptr_t)addr;
			auto data_value = (std::uintptr_t)base_t::get_data();
			if (addr_value < data_value) {
				return false;
			} if ((addr_value & ((std::uintptr_t)chunk_size - 1)) != 0) {
				return false;
			} return ((addr_value - data_value) >> chunk_size) < base_t::get_capacity();
		}
		
	private:
		attrs_t chunk_size{};
	};

	template<>
	class pool_ops_t<false> : public pool_descr_wrapper_t {
	public:
		using base_t = pool_descr_wrapper_t;
		using bd_t = typename base_t::bd_t;

		static constexpr bool is_pow2 = false;

		pool_ops_t(bd_t* descr = nullptr, attrs_t chunk_size_ = chunk_size_empty)
			: base_t(descr), chunk_size{chunk_size_} {}

		attrs_t get_chunk_size() const {
			return aux_pool_chunk_size<attrs_t>(chunk_size);
		}

		attrs_t get_chunk_size_enum() const {
			return chunk_size;
		}

		void* get_chunk_memory(attrs_t index) const {
			return (char*)base_t::get_data() + (std::uintptr_t)index * get_chunk_size();
		}

		attrs_t get_chunk_index(void* chunk) const {
			assert(has_chunk(chunk));
			auto diff = (std::uintptr_t)chunk - (std::uintptr_t)base_t::get_data();
			return (attrs_t)(diff / (std::uintptr_t)chunk_size);
		}

		void* refine_chunk_memory(void* chunk) const {
			auto chunk_value = (std::uintptr_t)chunk;
			auto data_value = (std::uintptr_t)get_data();
			if (chunk_value < data_value) {
				return nullptr;
			} return (void*)(chunk_value - chunk_value % (std::uintptr_t)chunk_size);
		}

		bool has_chunk(void* addr) const {
			auto addr_value = (std::uintptr_t)addr;
			auto data_value = (std::uintptr_t)get_data();
			return addr_value >= data_value && addr_value % chunk_size == 0 &&
				(addr_value - data_value) / chunk_size < base_t::get_capacity();
		}

	private:
		attrs_t chunk_size{};
	};

	template<bool is_pow2>
	class basic_pool_ops_t : public pool_ops_t<is_pow2> {
	public:
		using base_t = pool_ops_t<is_pow2>;

		void* acquire_chunk() {
			attrs_t head = base_t::get_head();
			if (head != head_empty) {
				void* chunk = base_t::get_chunk_memory(head);
				base_t::set_head(((pool_hdr_t*)chunk)->next);
				return chunk;
			} if (base_t::has_capacity()) {
				return base_t::get_chunk_memory(base_t::inc_used());
			} return nullptr; // no more chunks
		}

		void* peek_chunk() const {
			attrs_t head = base_t::get_head();
			if (head != head_empty) {
				return base_t::get_chunk_memory(head);
			} return nullptr;
		}

		void release_chunk(void* chunk) {
			assert(base_t::has_chunk(chunk));
			((pool_hdr_t*)chunk)->next = base_t::get_head();
			base_t::set_head(base_t::get_chunk_index(chunk));
		}
	};

	template<bool is_pow2>
	class basic_pool_wrapper_t : public basic_pool_ops_t<is_pow2> {
	public:
		using base_t = basic_pool_ops_t<is_pow2>;

		void* acquire_chunk() {
			void* chunk = base_t::acquire_chunk();
			if (chunk) {
				base_t::inc_count();
			} return chunk;
		}

		void release_chunk(void* chunk) {
			base_t::release_chunk(chunk);
			base_t::dec_count();
		}
	};

	class pool_wrapper_t : public basic_pool_wrapper_t<true> {};

	class aux_pool_wrapper_t : public basic_pool_wrapper_t<false> {};

	class byte_pool_wrapper_t : public basic_pool_wrapper_t<true> {
	public:
		using base_t = basic_pool_wrapper_t<true>;
		using bd_t = typename base_t::bd_t;

		inline byte_pool_wrapper_t(bd_t* descr) : base_t(descr, type_to_chunk_size<byte_pool_t, attrs_t>()) {}

		inline void* acquire_chunk() {
			byte_pool_t* pool = (byte_pool_t*)base_t::peek_chunk();
			if (pool) {
				void* chunk = pool->acquire_chunk();
				if (pool->full()) {
					base_t::acquire_chunk();
					base_t::inc_count();
				} return chunk;
			} return nullptr;
		}

		inline void* peek_chunk() const {
			byte_pool_t* pool = (byte_pool_t*)base_t::peek_chunk();
			if (pool) {
				return pool->peek_chunk();
			} return nullptr;
		}

		inline void release_chunk(void* chunk) {
			byte_pool_t* pool = (byte_pool_t*)base_t::refine_chunk_memory(chunk);
			if (pool) {
				pool->release_chunk(chunk);
				if (pool->empty()) {
					base_t::release_chunk(chunk);
					base_t::dec_count();
				}
			}
		}

		inline bool has_chunk(void* chunk) const {
			byte_pool_t* pool = (byte_pool_t*)base_t::refine_chunk_memory(chunk);
			if (pool) {
				return pool->has_chunk(chunk);
			} return false;
		}
	};

	class raw_wrapper_t {
	public:
		using bd_t = block_descr_t;

		inline raw_wrapper_t(bd_t* descr_) : descr{descr_} {}

		inline bool has_addr(void* addr) const {
			return descr->has_addr(addr);
		}

		inline void* get_data() const {
			return descr->data;
		}

		inline std::size_t get_size() const {
			return descr->size;
		}

		inline bd_t* get_descr() const {
			return descr;
		}

	private:
		block_descr_t* descr{};
	};
}