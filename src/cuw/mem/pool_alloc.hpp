#pragma once

// NOTE : this file is under construction

#include "core.hpp"
#include "traits.hpp"
#include "alloc_tag.hpp"
#include "cached_alloc.hpp"
#include "alloc_entries.hpp"

namespace cuw::mem {
	using block_descr_entry_t = block_pool_entry_t;

	namespace impl {
		template<class basic_alloc_t, bool use_alloc_cache>
		class pool_alloc_adapter_t;

		template<class basic_alloc_t>
		class pool_alloc_adapter_t<basic_alloc_t, false> : public basic_alloc_t {
		public:
			using base_t = basic_alloc_t;

			std::tuple<void*, std::size_t> allocate_ext(std::size_t size, cached_alloc_flags_t flags) {
				return {base_t::allocate(size), size};
			}
		};

		template<class base_alloc_t>
		class pool_alloc_adapter_t<basic_alloc_t, true> : public cached_alloc_t<basic_alloc_t> {
		public:
			using base_t = cached_alloc_t<basic_alloc_t>;

			std::tuple<void*, std::size_t> allocate_ext(std::size_t size, cached_alloc_flags_t flags) {
				return base_t::allocate_ext(size, flags);
			}
		};

		template<class pool_type_t, auto first_chunk, auto last_chunk>
		class pools_t {
		public:
			static_assert(std::is_same_v<decltype(first_chunk), decltype(last_chunk)>);
			static_assert((attrs_t)first_chunk <= (attrs_t)last_chunk);

			static constexpr attrs_t total_pools = (attrs_t)last_chunk - (attrs_t)first_chunk + 1;

			using enum_t = std::underlying_type_t<decltype(first_chunk)>;

		private:
			static enum_t index_to_enum(attrs_t index) {
				return enum_t{(std::size_t)first_chunk + index};
			}

		public:
			pools_t() {
				for (int i = 0; i < total_pools; i++) {
					pools[i] = pool_type_t(index_to_enum(i));
				}
			}

			auto begin() {
				return std::begin(pools);
			}

			auto end() {
				return std::end(pools);
			}

			// can return end() (not valid iter)
			auto find(attrs_t size) {
				return std::lower_bound(begin(), end(), size, [&] (auto& pool, auto& value) {
					return pool.get_chunk_size() < value;
				});
			}

			void adopt(pools_t& another) {
				for (int i = 0; i < total_pools; i++) {
					pools[i].adopt(another.pools[i]);
				}
			}

		private:
			pool_type_t pools[total_pools] = {};
		};

		template<class bin_type_t, attrs_t base_size, attrs_t total_bins>
		class raw_bins_t {
		public:
			raw_bins_t() {
				for (attrs_t i = 1; i < total_bins; i++) {
					size_limits[i - 1] = base_size << i;
				}
			}

			auto begin() {
				return std::begin(bins);
			}

			auto end() {
				return begin() + total_bins - 1; // last bin serves the rest of the allocations
			}

			// bin stores allocations that are less than thresh value
			// always returns valid iter
			auto find(attrs_t size) {
				auto it1 = std::begin(size_limits);
				auto it2 = std::begin(size_limits) + total_bins - 1;
				auto it = std::lower_bound(it1, it2, size, [&] (auto& size_limit, auto& value) {
					return size_limit <= value;
				});
				return bins[it - it1];
			}

			void insert(bd_t* bd) {
				find(bd->get_size())->insert(bd);
			}

			void adopt(raw_bins_t& another) {
				for (int i = 0; i < total_bins; i++) {
					bins[i].adopt(another.bins[i]);
				}
			}

		private:
			bin_type_t bins[total_bins];
			attrs_t size_limits[total_bins];
		};
	}

	template<class base_alloc_t>
	using pool_alloc_adapter_t = impl::pool_alloc_adapter_t<base_alloc_t, base_alloc_t::use_alloc_cache>;

	template<class base_alloc_t>
	class pool_alloc_t : public pool_alloc_adapter_t<base_alloc_t> {
	public:
		using tag_t = mem_alloc_tag_t;

		using base_t = pool_alloc_adapter_t<base_alloc_t>;

		using bp_t = block_pool_t;

		using bd_t = block_descr_t;
		using bd_entry_t = block_descr_entry_t;

		static_assert(has_sysmem_alloc_tag_v<base_t>);

	private:
		using pool_bytes_t = byte_pool_entry_t<>;
		using pool_t = pool_entry_t<>;
		using aux_pool_t = aux_pool_entry_t<>;
		using raw_bin_t = raw_entry_t<>;

		using pools_t = impl::pools_t<pool_t, base_t::alloc_pool_first_chunk, base_t::alloc_pool_last_chunk>;
		using aux_pools_t = impl::pools_t<aux_pool_t, base_t::alloc_aux_pool_first_chunk, base_t::alloc_aux_pool_last_chunk>;
		using raw_bins_t = impl::raw_bins_t<raw_bin_t, base_t::alloc_raw_base_size, base_t::alloc_total_raw_bins>;

	public:
		// addr_cache is external to pool_allocator
		pool_alloc_t() = default;

		pool_alloc_t(const pool_alloc_t&) = delete;
		pool_alloc_t(pool_alloc_t&&) = delete;

		~pool_alloc_t() {
			bd_entry.release_all([&] (void* data, std::size_t size) {
				base_t::free(data, size);
			});

			auto release_func = [&] (void* block, attrs_t offset, void* data, attrs_t size) {
				base_t::free(data, size);
				free_bd(block, offset);
			};
			
			byte_pool.release_all(addr_cache, release_func);
			for (auto& pool : pools) {
				pool.release_all(addr_cache, release_func);
			} for (auto& pool : aux_pools) {
				pool.release_all(addr_cache, release_func);
			} for (auto& bin : raw_bins) {
				bin.release_all(addr_cache, release_func);
			}
		}

		pool_alloc_t& operator = (const pool_alloc_t&) = delete;
		pool_alloc_t& operator = (pool_alloc_t&&) = delete; 

		void adopt(pool_alloc_t& another) {
			assert(this != &another);

			bd_entry.adopt(another.bd_entry);

			addr_cache.adopt(another.addr_cache);

			byte_pool.adopt(another.byte_pool);
			pools.adopt(another.pools);
			aux_pools.adopt(another.aux_pools);
			raw_bins.adopt(another.raw_bins);
		}

	private:
		template<class int_t>
		void assert_alignment(int_t value) {
			assert(is_alignment(value) && value <= base_t::get_page_size());
		}

		template<class int_t>
		int_t adjust_alignment(int_t value) {
			assert_alignment();
			if (value == 0) {
				value = base_t::alloc_base_alignment;
			} return std::min(value, (int_t)base_t::get_page_size());
		}

	private:
		// returns (memory for block description, offset from primary block)
		std::tuple<void*, attrs_t> alloc_bd() {
			if (auto [ptr, offset] = bd_entry.acquire(); ptr) {
				return {ptr, offset};
			}

			std::size_t pool_size = base_t::alloc_block_pool_size;
			if (void* pool_mem = base_t::malloc(pool_size); pool_mem) {
				bd_entry.create_pool(pool_mem, pool_size);
				return bd_entry.acquire();
			} return {nullptr, head_empty};
		}

		// deallocates description, does not free associated memory
		void free_bd(void* descr, attrs_t offset) {
			if (bp_t* released = bd_entry.release(descr, offset); released) {
				bd_entry.finish_release(released);
				base_t::free(released->get_data(), released->get_size());
			}
		}

	private:
		template<class _pool_t>
		void create_empty(_pool_t& pool) {
			auto [bd, offset] = alloc_bd();
			if (!bd) {
				return nullptr;
			}

			auto [pool_size, pool_capacity] = pool.get_next_pool_params(base_t::alloc_min_pool_power, base_t::alloc_max_pool_power);

			void* pool_data = base_t::malloc(size);
			if (!pool_data) {
				free_bd(bd, offset);
				return nullptr;
			}
			
			pool.create_empty(addr_cache, bd, offset, pool_size, pool_capacity, pool_data);
		}

		template<class _entry_t>
		void finish_release(_entry_t& entry, bd_t* bd) {
			if (bd) {
				entry.finish_release(addr_cache, bd, [&] (void* block, attrs_t offset, void* data, attrs_t size) {
					base_t::free(data, size);
					free_bd(block, offset);
				});
			}
		}

		template<class _pool_t>
		void* alloc_pool(_pool_t& pool) {
			if (void* ptr = pool.acquire(); ptr) {
				return ptr;
			}

			create_empty(pool);

			void* ptr = pool.acquire();
			assert(ptr);
			return ptr;
		}

		template<class _pool_t>
		void free_pool(_pool_t& pool, void* ptr) {
			finish_release(addr_cache, pool.release(addr_cache, ptr, base_t::alloc_pool_cache_lookups));
		}

		template<class _pool_t>
		void free_pool(_pool_t& pool, void* ptr, bd_t* bd) {
			finish_release(addr_cache, pool.release(ptr, bd));
		}

	private:
		void* alloc_byte() {
			alloc_pool(byte_pool);
		}

		void free_byte(void* ptr) {
			free_pool(byte_pool, ptr);
		}

		void free_byte(void* ptr, bd_t* bd) {
			free_pool(byte_pool, ptr, bd);
		}

		// allocates memory only if memory is allocated in one of the pools
		// true means that we tried to allocate from pool
		// pointer can be null 
		template<class iter1_t, class iter2_t>
		std::tuple<void*, bool> alloc_pool_choice(std::size_t size_aligned, std::size_t alignment,
								iter1_t pool, iter1_t end, iter2_t aux_pool, iter2_t aux_end) {
			assert_alignment(alignment);

			if (size_aligned == 1) {
				return {alloc_byte(), true};
			} if (pool != end && aux_pool != aux_end) {
				auto pool_chunk = pool->get_chunk_size();
				auto aux_pool_chunk = aux_pool->get_chunk_size();
				if (aux_pool_chunk < pool_chunk && alignment <= aux_pool->get_max_alignment()) {
					return {alloc_pool(*aux_pool), true};
				} return {alloc_pool(*pool), true};
			} if (pool != end || aux_pool != aux_end) {
				if (pool != end) {
					return {alloc_pool(*pool), true};
				} return {alloc_pool(*aux_pool), true};
			} return {nullptr, false};
		}

		template<class iter1_t, class iter2_t>
		bool free_pool_choice(void* ptr, std::size_t size_aligned, std::size_t alignment,
					iter1_t pool, iter1_t end, iter2_t aux_pool, iter2_t aux_end) {
			assert_alignment(alignment);

			if (size_aligned == 1) {
				free_byte(ptr);
				return true;
			} if (pool != end() && aux_pool != aux_end) {
				auto pool_chunk = pool->get_chunk_size();
				auto aux_pool_chunk = aux_pool->get_chunk_size();
				if (aux_pool_chunk < pool_chunk && alignment <= aux_pool->get_max_alignment()) {
					free_pool(*aux_pool, ptr);
					return true;
				}
				free_pool(*pool, ptr);
				return true;
			} if (pool != pools.end() || aux_pool != aux_pools.end()) {
				if (pool != pools.end()) {
					free_pool(*pool, ptr);
					return true;
				}
				free_pool(*aux_pool, ptr);
				return true;
			} return false;
		}

		template<class _raw_bin_t>
		void* alloc_raw(_raw_bin_t& bin, std::size_t size, std::size_t alignment) {
			alignment = adjust_alignment(alignment);

			auto [bd, offset] = alloc_bd();
			if (!bd) {
				return nullptr;
			}

			std::size_t size_aligned = align_value(size, alignment);
			void* data = base_t::malloc(size_aligned);
			if (!data) {
				free_bd(bd, offset);
				return nullptr;
			}

			bin.create(addr_cache, bd, offset, size, alignment, data);
			return data;
		}

		template<class _raw_bin_t>
		void free_raw(target_bin_t& bin, void* ptr) {
			finish_release(addr_cache, bin.release(addr_cache, ptr, base_t::alloc_raw_cache_lookups));
		}

		template<class _raw_bin_t>
		void free_raw(_raw_bin_t& bin, bd_t* bd) {
			finish_release(addr_cache, bin.release(addr_cache, bd));
		}

		template<class _raw_bin_t>
		bd_t* extract_raw(target_bin_t& bin, void* ptr) {
			return bin.extract(addr_cache, ptr, base_t::alloc_raw_cache_lookups);
		}

		template<class _raw_bin_t>
		void put_back_raw(_raw_bin_t& bin, bd_t* bd) {
			bin.put_back(addr_cache, bd);
		}

		void* realloc_raw(void* old_ptr, std::size_t old_size, std::size_t alignment, std::size_t new_size) {
			assert_alignment(alignment);

			auto old_bin = raw_bins.find(old_size);
			auto new_bin = raw_bins.find(new_size);

			bd_t* extracted = extract_raw(*old_bin, old_ptr);
			assert(alignment == extracted->get_alignment());

			std::size_t old_size_aligned = align_value(old_size, alignment);
			std::size_t new_size_aligned = align_value(new_size, alignment);

 			void* new_memory = base_t::realloc(extracted->get_data(), old_size_aligned, new_size_aligned);
			if (!new_memory) {
				put_back_raw(*old_bin, extracted);
				return nullptr;
			}

			if (new_memory != extracted->get_data()) {
				extracted->set_size(new_size);
				extracted->set_data(new_memory);
			}
			put_back_raw(*new_bin, extracted);
			return new_memory;
		}

	private:
		void* alloc42(std::size_t size) {
			std::size_t alignment = size != 1 ? 0 : 1;
			return alloc42(size, alignment);
		}

		void* alloc42(std::size_t size, std::size_t alignment) {
			alignment = adjust_alignment(alignment);

			std::size_t size_aligned = align_value(size, alignment);

			auto pool = pools.find(size_aligned);
			auto aux_pool = aux_pools.find(size_aligned);
			auto [ptr, in_pool] = alloc_pool_choice(size_aligned, alignment, pool, pools.end(), aux_pool, aux_pools.end()); 
			if (in_pool) {
				return ptr;
			} return alloc_raw(*raw_bins.find(size), size, alignment);
		}

		void free42(void* ptr) {
			if (bd_t* bd = addr_cache.find(ptr); bd) {
				free42(bd, ptr);
			} abort();
		}

		void free42(bd_t* bd, void* ptr) {
			using enum block_type_t;

			switch (block_type_t{bd->get_type()}) {
				case PoolBytes: {
					free_byte(ptr, bd);
					break;
				} case Pool: {
					attrs_t chunk_size = pool_chunk_size<attrs_t>(bd->get_chunk_size());
					if (auto pool = pools.find(chunk_size); pool != pools.end()) {
						free_pool(*pool, ptr, bd);
						return;
					} abort();
				} case PoolAux: {
					attrs_t chunk_size = aux_pool_chunk_size<attrs_t>(bd->get_chunk_size());
					if (auto pool = aux_pools.find(chunk_size); pool != aux_pools.end()) {
						free_pool(*pool, ptr, bd);
						return;
					} abort();
				} case Raw: {
					free_raw(*raw_bins.find(bd->get_size()), bd);
					return;
				} default: {
					abort();
				}
			}
		}

		void free42(void* ptr, std::size_t size, std::size_t alignment) {
			alignment = adjust_alignment(alignment);

			std::size_t size_aligned = align_value(size, alignment);
			auto pool = pools.find(size_aligned);
			auto aux_pool = aux_pools.find(size_aligned);
			if (free_pool_choice(ptr, size_aligned, alignment, pool, pools.end(), aux_pool, aux_pool.end())) {
				return;
			} free_raw(*raw_bins.find(size), ptr);
		}

		void* realloc42(void* old_ptr, std::size_t new_size) {
			using enum block_type_t;

			bd_t* bd = addr_cache.find(old_ptr);
			if (!bd) {
				return nullptr;
			}

			std::size_t old_size = 0;
			std::size_t alignment = 0;
			switch (block_type_t{bd->get_type()}) {
				case PoolBytes: {
					old_size = 1;
					alignment = 1;
					break;
				} case Pool: {
					old_size = pool_chunk_size(bd->get_chunk_size());
					alignment = pool_alignment(bd->get_chunk_size()); // will be adjusted further
					break;
				} case PoolAux: {
					old_size = aux_pool_chunk_size(bd->get_chunk_size());
					alignment = aux_pool_alignment(bd->get_chunk_size()); // will be adjusted further
					break;
				} case Raw: {
					old_size = bd->get_size();
					alignment = bd->get_alignment(); // will be adjusted further
					break;
				} default: {
					abort();
				}
			} return realloc42(old_ptr, old_size, alignment, new_size);
		}

		void* realloc42(void* old_ptr, std::size_t old_size, std::size_t alignment, std::size_t new_size) {
			alignment = adjust_alignment(alignment);

			std::size_t old_size_aligned = align_value(old_size, alignment);
			std::size_t new_size_aligned = align_value(new_size, alignment);
			if (old_size_aligned == new_size_aligned) {
				return old_ptr;
			}

			auto old_pool = pools.find(old_size_aligned);
			auto old_aux_pool = aux_pools.find(old_size_aligned);
			auto new_pool = pools.find(new_size_aligned);
			auto new_aux_pool = aux_pools.find(new_size_aligned);

			bool old_in_pool = old_size_aligned == 1 || old_pool != pools.end() || old_aux_pool != aux_pools.end();
			bool new_in_pool = new_size_aligned == 1 || new_pool != pools.end() || new_aux_pool != aux_pools.end(); 

			// pool-pool transfer
			if (old_in_pool && new_in_pool) {
				if (old_pool != pools.end() && old_pool == new_pool ||
					old_aux_pool != aux_pools.end() && old_aux_pool == new_aux_pool) {
					return old_ptr;
				}

				auto [new_ptr, in_pool] = alloc_pool_choice(new_size_aligned,
													new_pool, pools.end(), new_aux_pool, aux_pools.end());
				if (!new_ptr) {
					return nullptr;
				}

				memcpy(new_ptr, old_ptr, std::min(old_size, new_size));
				free_pool_choice(old_ptr, old_size_aligned, old_pool, pools.end(), old_aux_pool, aux_pools.end());
				return new_ptr;
			}

			// pool-raw transfer
			if (old_in_pool) {
				void* new_ptr = alloc_raw(new_size_aligned);
				if (!new_ptr) {
					return nullptr;
				}

				memcpy(new_ptr, old_ptr, std::min(old_size, new_size));
				free_pool_choice(old_ptr, old_size_aligned, old_pool, pools.end(), old_aux_pool, aux_pools.end());
				return new_ptr;
			}

			// raw-pool transfer
			if (new_in_pool) {
				auto [new_ptr, in_pool] = alloc_pool(new_size_aligned,
												new_pool, pools.end(), new_aux_pool, aux_pools.end());
				if (!new_ptr) {
					return nullptr;
				}

				memcpy(new_ptr, old_ptr, std::min(old_size, new_size));
				free_raw(*raw_bins.find(old_size_aligned), old_ptr); // TODO : raw allocations & alignment
				return new_ptr;
			}

			// raw-raw transfer
			return realloc_raw(old_ptr, old_size, alignment, new_size);
		}

		void* zero_alloc() {
			static int zero_allocation;
			return &zero_allocation;
		}

	public: // standart API
		void* malloc(std::size_t size) {
			if (size == 0){
				return zero_alloc();
			} return alloc42(size, alignment);
		}

		void* realloc(void* ptr, std::size_t new_size) {
			if (!ptr) {
				return alloc(new_size);
			} if (new_size == 0) {
				free42(ptr);
				return zero_alloc();
			} return realloc42(ptr, new_size);
		}

		void free(void* ptr) {
			if (!ptr || ptr == zero_alloc()) {
				return;
			} free42(ptr);
		}

	public: // extension API
		void* malloc(std::size_t size, std::size_t alignment, flags_t flags = 0) {
			if (size == 0) {
				return zero_alloc();
			} return alloc42(size, alignment);
		}

		void* realloc(void* ptr, std::size_t old_size, std::size_t new_size, std::size_t alignment, flags_t flags = 0) {
			if (!ptr) {
				return alloc(new_size, alignment, flags);
			} if (new_size == 0) {
				free(ptr, old_size, alignment, flags);
				return zero_alloc();
			} return realloc42(ptr, old_size, new_size, alignment);
		}

		void free(void* ptr, std::size_t size, std::size_t alignment, flags_t flags = 0) {
			if (!ptr || ptr == zero_alloc()) {
				return;
			} return free42(ptr, size, alignment);
		}

	private:
		bd_entry_t bd_entry{};

		addr_cache_t addr_cache{}; // common addr cache for all allocations

		pool_bytes_t byte_pool{};
		pools_t pools{};
		aux_pools_t aux_pools{};
		raw_bins_t raw_bins{};
	};
}