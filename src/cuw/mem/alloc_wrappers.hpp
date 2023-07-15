#pragma once

#include "core.hpp"
#include "alloc_descr.hpp"

namespace cuw::mem {
	// now several words about pools:
	// size = 2,4,.., alignment = 2,4,.. => ordinary pool
	// every pool will use 14-bit head that will serve as an index of a chunk
	// for single allocation pool_chunk_size becomes alignment
	// each pool can store not more than 2^14 - 1 chunks

	class pool_ops_t : public alloc_descr_wrapper_t {
	public:
		using base_t = alloc_descr_wrapper_t;
		using ad_t = alloc_descr_t;

		pool_ops_t(ad_t* descr = nullptr, attrs_t _chunk_size_log2 = 0, attrs_t _alignment = 0)
			: base_t(descr), chunk_size_log2{_chunk_size_log2} , alignment{_alignment} {}

		attrs_t get_chunk_size() const {
			return value_to_pow2(chunk_size_log2);
		}

		attrs_t get_chunk_size_log2() const {
			return chunk_size_log2;
		}

		attrs_t get_alignment() const {
			return alignment;
		}

		void* get_chunk_memory(attrs_t index) const {
			return (char*)base_t::get_data() + ((std::uintptr_t)index << chunk_size_log2);
		}

		attrs_t get_chunk_index(void* chunk) const {
			assert(has_chunk(chunk));
			auto diff = (std::uintptr_t)chunk - (std::uintptr_t)base_t::get_data();
			return (attrs_t)(diff >> (std::uintptr_t)chunk_size_log2);
		}

		void* refine_chunk_memory(void* chunk) const {
			auto chunk_value = (std::uintptr_t)chunk;
			auto data_value = (std::uintptr_t)base_t::get_data();
			if (chunk_value < data_value) {
				return nullptr;
			}
			return (void*)(((chunk_value - data_value) & ~((std::uintptr_t)get_alignment() - 1)) + data_value);
		}

		bool has_chunk(void* addr) const {
			auto addr_value = (std::uintptr_t)addr;
			auto data_value = (std::uintptr_t)base_t::get_data();
			if (addr_value < data_value) {
				return false;
			}
			if ((addr_value & ((std::uintptr_t)get_alignment() - 1)) != 0) {
				return false;
			}
			return ((addr_value - data_value) >> (std::uintptr_t)chunk_size_log2) < base_t::get_capacity();
		}
		
	private:
		attrs_t chunk_size_log2{};
		attrs_t alignment{};
	};

	class basic_pool_ops_t : public pool_ops_t {
	public:
		using base_t = pool_ops_t;
		using base_t::base_t;
		
		[[nodiscard]] void* acquire_chunk() {
			if (attrs_t head = base_t::get_head(); head != alloc_descr_head_empty) {
				void* chunk = base_t::get_chunk_memory(head);
				base_t::set_head(((pool_hdr_t*)chunk)->next);
				return chunk;
			}
			return acquire_unused_chunk();
		}

		// peeks next unused chunk
		void* peek_chunk() const {
			if (attrs_t head = base_t::get_head(); head != alloc_descr_head_empty) {
				return base_t::get_chunk_memory(head);
			}
			return nullptr;
		}

		void release_chunk(void* chunk) {
			assert(base_t::has_chunk(chunk));
			((pool_hdr_t*)chunk)->next = base_t::get_head();
			base_t::set_head(base_t::get_chunk_index(chunk));
		}

		[[nodiscard]] void* acquire_unused_chunk() {
			if (base_t::has_capacity()) {
				return base_t::get_chunk_memory(base_t::inc_used());
			}
			return nullptr; // no more chunks
		}
	};

	class basic_pool_wrapper_t : public basic_pool_ops_t {
	public:
		using base_t = basic_pool_ops_t;
		using base_t::base_t;

		void* acquire_chunk() {
			void* chunk = base_t::acquire_chunk();
			if (chunk) {
				base_t::inc_count();
			}
			return chunk;
		}

		void release_chunk(void* chunk) {
			base_t::release_chunk(chunk);
			base_t::dec_count();
		}

		void* acquire_unused_chunk() {
			void* chunk = base_t::acquire_unused_chunk();
			if (chunk) {
				base_t::inc_count();
			}
			return chunk;
		}
	};

	using pool_wrapper_t = basic_pool_wrapper_t;

	class raw_wrapper_t {
	public:
		using ad_t = alloc_descr_t;

		raw_wrapper_t(ad_t* descr_) : descr{descr_} {}

		bool has_addr(void* addr) const {
			return descr->has_addr(addr);
		}

		void* get_data() const {
			return descr->data;
		}

		std::size_t get_size() const {
			return descr->size;
		}

		ad_t* get_descr() const {
			return descr;
		}

	private:
		ad_t* descr{};
	};
}