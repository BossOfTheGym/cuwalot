#pragma once

#include "core.hpp"
#include "alloc_descr.hpp"

// TODO : under construction

namespace cbt::mem {
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
		using ad_t = alloc_descr_t;

		pool_descr_wrapper_t(ad_t* _descr = nullptr) : descr{_descr} {}

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

		inline void set_descr(ad_t* value) {
			descr = value;
		} 

		inline ad_t* get_descr() const {
			return descr;
		}

	private:
		ad_t* descr{};
	};

	// pool_chunk is power of two
	class pool_ops_t : public pool_descr_wrapper_t {
	public:
		using base_t = pool_descr_wrapper_t;
		using ad_t = typename base_t::ad_t;

		inline pool_ops_t(ad_t* descr = nullptr, attrs_t chunk_size_ = chunk_size_empty)
			: base_t(descr), chunk_size{chunk_size_} {}

		inline attrs_t get_chunk_size() const {
			return pool_chunk_size<attrs_t>(chunk_size);
		}

		inline attrs_t get_chunk_size_enum() const {
			return chunk_size;
		}

		inline void* get_chunk_memory(attrs_t index) const {
			return (char*)base_t::get_data() + ((std::uintptr_t)index << chunk_size);
		}

		inline attrs_t get_chunk_index(void* chunk) const {
			assert(has_chunk(chunk));
			auto diff = (std::uintptr_t)chunk - (std::uintptr_t)base_t::get_data();
			return (attrs_t)(diff >> (std::uintptr_t)chunk_size);
		}

		inline void* refine_chunk_memory(void* chunk) const {
			auto chunk_value = (std::uintptr_t)chunk;
			auto data_value = (std::uintptr_t)base_t::get_data();
			if (chunk_value < data_value) {
				return nullptr;
			} return (void*)(chunk_value & ((std::uintptr_t)get_chunk_size() - 1));
		}

		inline bool has_chunk(void* addr) const {
			auto addr_value = (std::uintptr_t)addr;
			auto data_value = (std::uintptr_t)base_t::get_data();
			if (addr_value < data_value) {
				return false;
			} if ((addr_value & ((std::uintptr_t)get_chunk_size() - 1)) != 0) {
				return false;
			} return ((addr_value - data_value) >> (std::uintptr_t)chunk_size) < base_t::get_capacity();
		}
		
	private:
		attrs_t chunk_size{};
	};

	class basic_pool_ops_t : public pool_ops_t {
	public:
		using base_t = pool_ops_t;

		inline void* acquire_chunk() {
			if (attrs_t head = base_t::get_head(); head != head_empty) {
				void* chunk = base_t::get_chunk_memory(head);
				base_t::set_head(((pool_hdr_t*)chunk)->next);
				return chunk;
			} if (base_t::has_capacity()) {
				return base_t::get_chunk_memory(base_t::inc_used());
			} return nullptr; // no more chunks
		}

		inline void* peek_chunk() const {
			if (attrs_t head = base_t::get_head(); head != head_empty) {
				return base_t::get_chunk_memory(head);
			} return nullptr;
		}

		inline void release_chunk(void* chunk) {
			assert(base_t::has_chunk(chunk));
			((pool_hdr_t*)chunk)->next = base_t::get_head();
			base_t::set_head(base_t::get_chunk_index(chunk));
		}
	};

	class basic_pool_wrapper_t : public basic_pool_ops_t {
	public:
		using base_t = basic_pool_ops_t;

		inline void* acquire_chunk() {
			void* chunk = base_t::acquire_chunk();
			if (chunk) {
				base_t::inc_count();
			} return chunk;
		}

		inline void release_chunk(void* chunk) {
			base_t::release_chunk(chunk);
			base_t::dec_count();
		}
	};

	using pool_wrapper_t = basic_pool_wrapper_t;

	class byte_pool_wrapper_t : public basic_pool_wrapper_t {
	public:
		using base_t = basic_pool_wrapper_t;
		using ad_t = typename base_t::ad_t;

		inline byte_pool_wrapper_t(ad_t* descr) : base_t(descr, type_to_chunk_size<byte_pool_t, attrs_t>()) {}

		inline void* acquire_chunk() {
			if (byte_pool_t* pool = (byte_pool_t*)base_t::peek_chunk()) {
				void* chunk = pool->acquire_chunk();
				if (pool->full()) {
					base_t::acquire_chunk();
					base_t::inc_count();
				} return chunk;
			} return nullptr;
		}

		inline void* peek_chunk() const {
			if (byte_pool_t* pool = (byte_pool_t*)base_t::peek_chunk()) {
				return pool->peek_chunk();
			} return nullptr;
		}

		inline void release_chunk(void* chunk) {
			if (byte_pool_t* pool = (byte_pool_t*)base_t::refine_chunk_memory(chunk)) {
				pool->release_chunk(chunk);
				if (pool->empty()) {
					base_t::release_chunk(chunk);
					base_t::dec_count();
				}
			}
		}

		inline bool has_chunk(void* chunk) const {
			if (byte_pool_t* pool = (byte_pool_t*)base_t::refine_chunk_memory(chunk)) {
				return pool->has_chunk(chunk);
			} return false;
		}
	};

	class raw_wrapper_t {
	public:
		using ad_t = alloc_descr_t;

		inline raw_wrapper_t(ad_t* descr_) : descr{descr_} {}

		inline bool has_addr(void* addr) const {
			return descr->has_addr(addr);
		}

		inline void* get_data() const {
			return descr->data;
		}

		inline std::size_t get_size() const {
			return descr->size;
		}

		inline ad_t* get_descr() const {
			return descr;
		}

	private:
		ad_t* descr{};
	};
}