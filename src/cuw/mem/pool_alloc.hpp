#pragma once

// NOTE : this file is under construction

#include "core.hpp"
#include "alloc_tag.hpp"
#include "block_pool.hpp"
#include "alloc_traits.hpp"
#include "cached_alloc.hpp"
#include "alloc_entries.hpp"

namespace cuw::mem {
	using alloc_descr_entry_t = block_pool_entry_t;

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

		template<class basic_alloc_t>
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

			// void func(void* block, std::size_t offset, void* data, std::size_t size)
			template<class func_t>
			void release_all(func_t func) {
				for (auto& pool : pools) {
					pool.release_all(func);
				}
			}

			void adopt(pools_t& another, int merge_param) {
				for (int i = 0; i < total_pools; i++) {
					pools[i].adopt(another.pools[i], merge_param);
				}
			}

		private:
			pool_type_t pools[total_pools] = {};
		};

		template<class bin_type_t, attrs_t base_size, attrs_t total_bins>
		class raw_bins_t {
		public:
			using ad_t = alloc_descr_t;

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

			void insert(ad_t* ad) {
				find(ad->get_size())->insert(ad);
			}

			// void func(void* block, std::size_t offset, void* data, std::size_t size)
			template<class func_t>
			void release_all(func_t func) {
				for (auto& bin : bins) {
					bin.release_all(func);
				}
			}

			void adopt(raw_bins_t& another, int merge_param) {
				for (int i = 0; i < total_bins; i++) {
					bins[i].adopt(another.bins[i], merge_param);
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

		using ad_t = alloc_descr_t;
		using ad_entry_t = alloc_descr_entry_t;

		static_assert(has_sysmem_alloc_tag_v<base_t>);

	private:
		using pool_bytes_t = byte_pool_entry_t<>;
		using pool_t = pool_entry_t<>;
		using raw_bin_t = raw_entry_t<>;

		using pools_t = impl::pools_t<pool_t, base_t::alloc_pool_first_chunk, base_t::alloc_pool_last_chunk>;
		using raw_bins_t = impl::raw_bins_t<raw_bin_t, base_t::alloc_raw_base_size, base_t::alloc_total_raw_bins>;

	public:
		pool_alloc_t() = default;

		pool_alloc_t(const pool_alloc_t&) = delete;
		pool_alloc_t(pool_alloc_t&&) = delete;

		~pool_alloc_t() {
			auto release_func = [&] (void* block, attrs_t offset, void* data, attrs_t size) {
				base_t::deallocate(data, size);
				// free_descr(block, offset); // we can leak descrs here as all blocks will be freed anyways
			};
			byte_pool.release_all(addr_cache, release_func);
			pools.release_all(addr_cache, release_func);
			raw_bins.release_all(addr_cache, release_func);
			ad_entry.release_all([&] (void* data, std::size_t size) { base_t::deallocate(data, size); });
		}

		pool_alloc_t& operator = (const pool_alloc_t&) = delete;
		pool_alloc_t& operator = (pool_alloc_t&&) = delete; 

		void adopt(pool_alloc_t& another) {
			if (this == &another) {
				return;
			}
			ad_entry.adopt(another.ad_entry);
			addr_cache.adopt(another.addr_cache);
			byte_pool.adopt(another.byte_pool, base_t::alloc_pool_cache_lookups / 2);
			pools.adopt(another.pools, base_t::alloc_pool_cache_lookups / 2);
			raw_bins.adopt(another.raw_bins, base_t::alloc_raw_cache_lookups / 2);
		}

	private:
		template<class int_t>
		void assert_alignment(int_t value) {
			assert(is_alignment(value) && value <= base_t::get_page_size());
		}

		template<class int_t>
		int_t adjust_alignment(int_t value) {
			assert_alignment(value);
			if (value == 0) {
				value = base_t::alloc_base_alignment;
			} return std::min(value, (int_t)base_t::get_page_size());
		}

	private:
		// returns (memory for block description, offset from primary block)
		std::tuple<void*, attrs_t> alloc_descr() {
			if (auto [ptr, offset] = ad_entry.acquire(); ptr) {
				return {ptr, offset};
			}

			std::size_t pool_size = base_t::alloc_block_pool_size;
			if (void* pool_mem = base_t::allocate(pool_size)) {
				ad_entry.create_pool(pool_mem, pool_size);
				return ad_entry.acquire();
			} return {nullptr, block_pool_head_empty};
		}

		// deallocates description, does not free associated memory
		void free_descr(void* descr, attrs_t offset) {
			if (bp_t* released = ad_entry.release(descr, offset)) {
				ad_entry.finish_release(released, [&] (void* data, std::size_t size) { base_t::deallocate(data, size); });
			}
		}

	private:
		template<class _pool_t>
		bool create_empty(_pool_t& pool) {
			auto [ad, offset] = alloc_descr();
			if (!ad) {
				return false;
			}

			auto [pool_size, pool_capacity] = pool.get_next_pool_params(base_t::alloc_min_pool_power, base_t::alloc_max_pool_power);

			void* pool_data = base_t::allocate(pool_size);
			if (!pool_data) {
				free_descr(ad, offset);
				return false;
			}
			
			pool.create_empty(addr_cache, ad, offset, pool_size, pool_capacity, pool_data);
			return true;
		}

		template<class _entry_t>
		void finish_release(_entry_t& entry, ad_t* ad) {
			if (ad) {
				entry.finish_release(addr_cache, ad, [&] (void* block, attrs_t offset, void* data, attrs_t size) {
					base_t::deallocate(data, size);
					free_descr(block, offset);
				});
			}
		}

	private: // alignment must be adjusted beforehand
		template<class _pool_t>
		void* alloc_pool(_pool_t& pool) {
			if (void* ptr = pool.acquire()) {
				return ptr;
			} if (create_empty(pool)) {
				return pool.acquire();
			} return nullptr;
		}

		template<class _pool_t>
		void free_pool(_pool_t& pool, void* ptr) {
			finish_release(addr_cache, pool.release(addr_cache, ptr, base_t::alloc_pool_cache_lookups));
		}

		template<class _pool_t>
		void free_pool(_pool_t& pool, void* ptr, ad_t* ad) {
			finish_release(addr_cache, pool.release(ptr, ad));
		}

		void* alloc_byte() {
			return alloc_pool(byte_pool);
		}

		void free_byte(void* ptr) {
			free_pool(byte_pool, ptr);
		}

		void free_byte(void* ptr, ad_t* ad) {
			free_pool(byte_pool, ptr, ad);
		}

		template<class _raw_bin_t>
		void* alloc_raw(_raw_bin_t& bin, std::size_t size, std::size_t alignment) {
			auto [ad, offset] = alloc_descr();
			if (!ad) {
				return nullptr;
			}

			std::size_t size_aligned = align_value(size, alignment);
			void* data = base_t::allocate(size_aligned);
			if (!data) {
				free_descr(ad, offset);
				return nullptr;
			}

			bin.create(addr_cache, ad, offset, size, alignment, data);
			return data;
		}

		template<class _raw_bin_t>
		void free_raw(_raw_bin_t& bin, void* ptr) {
			finish_release(addr_cache, bin.release(addr_cache, ptr, base_t::alloc_raw_cache_lookups));
		}

		template<class _raw_bin_t>
		void free_raw(_raw_bin_t& bin, ad_t* ad) {
			finish_release(addr_cache, bin.release(addr_cache, ad));
		}

		template<class _raw_bin_t>
		ad_t* extract_raw(_raw_bin_t& bin, void* ptr) {
			return bin.extract(addr_cache, ptr, base_t::alloc_raw_cache_lookups);
		}

		template<class _raw_bin_t>
		void put_back_raw(_raw_bin_t& bin, ad_t* ad) {
			bin.put_back(addr_cache, ad);
		}

		void* realloc_raw(void* old_ptr, std::size_t old_size, std::size_t alignment, std::size_t new_size) {
			std::size_t old_size_aligned = align_value(old_size, alignment);
			std::size_t new_size_aligned = align_value(new_size, alignment);

			auto old_bin = raw_bins.find(old_size_aligned);
			auto new_bin = raw_bins.find(new_size_aligned);

			ad_t* extracted = extract_raw(*old_bin, old_ptr);
			if (!extracted) {
				return nullptr; // very bad
			} if (alignment == extracted->get_alignment()) {
				void* new_memory = base_t::reallocate(extracted->get_data(), old_size_aligned, new_size_aligned);
				if (!new_memory) {
					put_back_raw(*old_bin, extracted);
					return nullptr;
				} if (new_memory != extracted->get_data()) {
					extracted->set_size(new_size_aligned);
					extracted->set_data(new_memory);
				}
				put_back_raw(*new_bin, extracted);
				return new_memory;
			} return nullptr;
		}

	private:
		void* alloc42(std::size_t size) {
			std::size_t alignment = size != 1 ? 0 : 1;
			return alloc42(size, alignment);
		}

		void* alloc42(std::size_t size, std::size_t alignment) {
			alignment = adjust_alignment(alignment);

			std::size_t size_aligned = align_value(size, alignment);
			if (size_aligned == 1) {
				return alloc_byte();
			} if (auto pool = pools.find(size_aligned); pool != pools.end()) {
				return alloc_pool(*pool);
			} return alloc_raw(*raw_bins.find(size_aligned), size, alignment);
		}

		void free42(void* ptr) {
			if (ad_t* ad = addr_cache.find(ptr)) {
				free42(ad, ptr);
			} abort();
		}

		void free42(ad_t* ad, void* ptr) {
			using enum block_type_t;

			switch (block_type_t{ad->get_type()}) {
				case PoolBytes: {
					free_byte(ptr, ad);
					break;
				} case Pool: {
					attrs_t chunk_size = pool_chunk_size<attrs_t>(ad->get_chunk_size());
					if (auto pool = pools.find(chunk_size); pool != pools.end()) {
						free_pool(*pool, ptr, ad);
						return;
					} abort();
				} case Raw: {
					free_raw(*raw_bins.find(ad->get_size()), ad);
					return;
				} default: {
					abort();
				}
			}
		}

		void free42(void* ptr, std::size_t size, std::size_t alignment) {
			alignment = adjust_alignment(alignment);

			std::size_t size_aligned = align_value(size, alignment);
			if (size_aligned == 1) {
				free_byte(ptr);
			} else if (auto pool = pools.find(size_aligned); pool != pools.end()) {
				free_pool(*pool);
			} free_raw(*raw_bins.find(size_aligned), ptr);
		}

		void* realloc42(void* old_ptr, std::size_t new_size) {
			using enum block_type_t;

			ad_t* ad = addr_cache.find(old_ptr);
			if (!ad) {
				return nullptr;
			}

			std::size_t old_size = 0;
			std::size_t alignment = 0;
			switch (block_type_t{ad->get_type()}) {
				case PoolBytes: {
					old_size = 1;
					alignment = 1;
					break;
				} case Pool: {
					old_size = pool_chunk_size(ad->get_chunk_size());
					alignment = pool_alignment(ad->get_chunk_size()); // will be adjusted further
					break;
				} case Raw: {
					old_size = ad->get_size();
					alignment = ad->get_alignment(); // will be adjusted further
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
			auto new_pool = pools.find(new_size_aligned);
			bool old_in_pool = old_size_aligned == 1 || old_pool != pools.end();
			bool new_in_pool = new_size_aligned == 1 || new_pool != pools.end(); 

			// pool-pool transfer
			if (old_in_pool && new_in_pool) {
				if (old_pool != pools.end() && old_pool == new_pool) {
					return old_ptr; // same chunk_size
				}

				void* new_ptr{};
				if (new_size_aligned == 1) {
					new_ptr = alloc_byte();
				} else {
					new_ptr = alloc_pool(*new_pool);
				} if (!new_ptr) {
					return nullptr;
				}

				memcpy(new_ptr, old_ptr, std::min(old_size, new_size));
				if (old_size_aligned == 1) {
					free_byte(old_ptr);
				} else {
					free_pool(*old_pool, old_ptr);
				} return new_ptr;
			}

			// pool-raw transfer
			if (old_in_pool) {
				void* new_ptr = alloc_raw(new_size);
				if (!new_ptr) {
					return nullptr;
				}

				memcpy(new_ptr, old_ptr, std::min(old_size, new_size));
				if (old_size_aligned == 1) {
					free_byte(old_ptr);
				} else {
					free_pool(*old_pool, old_ptr);
				} return new_ptr;
			}

			// raw-pool transfer
			if (new_in_pool) {
				void* new_ptr{};
				if (new_size_aligned == 1) {
					new_ptr = alloc_byte();
				} else {
					new_ptr = alloc_pool(*new_pool);
				} if (!new_ptr) {
					return nullptr;
				}

				memcpy(new_ptr, old_ptr, std::min(old_size, new_size));
				free_raw(*raw_bins.find(old_size_aligned), old_ptr);
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
			} return alloc42(size);
		}

		void* realloc(void* ptr, std::size_t new_size) {
			if (!ptr) {
				return alloc42(new_size);
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
				return alloc42(new_size, alignment, flags);
			} if (new_size == 0) {
				free42(ptr, old_size, alignment, flags);
				return zero_alloc();
			} return realloc42(ptr, old_size, new_size, alignment);
		}

		void free(void* ptr, std::size_t size, std::size_t alignment, flags_t flags = 0) {
			if (!ptr || ptr == zero_alloc()) {
				return;
			} return free42(ptr, size, alignment);
		}

	private:
		ad_entry_t ad_entry{};
		alloc_descr_addr_cache_t addr_cache{}; // common addr cache for all allocations
		pool_bytes_t byte_pool{};
		pools_t pools{};
		raw_bins_t raw_bins{};
	};
}