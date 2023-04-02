#pragma once

#include "core.hpp"
#include "alloc_tag.hpp"
#include "block_pool.hpp"
#include "alloc_traits.hpp"
#include "cached_alloc.hpp"
#include "alloc_entries.hpp"

namespace cuw::mem {
	// TODO: wrap it up so it counts allocated blocks
	using alloc_descr_entry_t = block_pool_entry_t;

	namespace impl {
		template<class basic_alloc_t, bool use_alloc_cache>
		class pool_alloc_adapter_t;

		template<class basic_alloc_t>
		class pool_alloc_adapter_t<basic_alloc_t, false> : public basic_alloc_t {
		public:
			using base_t = basic_alloc_t;

			template<class ... args_t>
			pool_alloc_adapter_t(args_t&& ... args) : base_t(std::forward<args_t>(args)...) {}

			std::tuple<void*, std::size_t> allocate_ext(std::size_t size, cached_alloc_flags_t flags) {
				return {base_t::allocate(size), size};
			}
		};

		template<class basic_alloc_t>
		class pool_alloc_adapter_t<basic_alloc_t, true> : public cached_alloc_t<basic_alloc_t> {
		public:
			using base_t = cached_alloc_t<basic_alloc_t>;

			template<class ... args_t>
			pool_alloc_adapter_t(args_t&& ... args) : base_t(std::forward<args_t>(args)...) {}

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

			using enum_t = decltype(first_chunk);
			using ad_addr_cache_t = alloc_descr_addr_cache_t;

		private:
			static enum_t index_to_enum(attrs_t index) {
				return enum_t{(std::size_t)first_chunk + index};
			}

		public:
			pools_t(attrs_t max_alignment) {
				for (int i = 0; i < total_pools; i++) {
					auto enum_value = index_to_enum(i);
					auto alignment = std::min(pool_chunk_size((attrs_t)enum_value), max_alignment);
					pools[i] = pool_type_t(enum_value, alignment);
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
			void release_all(ad_addr_cache_t& addr_cache, func_t func) {
				for (auto& pool : pools) {
					pool.release_all(addr_cache, func);
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
			using ad_addr_cache_t = alloc_descr_addr_cache_t;

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
				return std::begin(bins) + (it - it1);
			}

			void insert(ad_t* ad) {
				find(ad->get_size())->insert(ad);
			}

			// void func(void* block, std::size_t offset, void* data, std::size_t size)
			template<class func_t>
			void release_all(ad_addr_cache_t& addr_cache, func_t func) {
				for (auto& bin : bins) {
					bin.release_all(addr_cache, func);
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

	template<class basic_alloc_t>
	using pool_alloc_adapter_t = impl::pool_alloc_adapter_t<basic_alloc_t, basic_alloc_t::use_alloc_cache>;

	template<class basic_alloc_t>
	class pool_alloc_t : public pool_alloc_adapter_t<basic_alloc_t> {
	public:
		using tag_t = mem_alloc_tag_t;

		using base_t = pool_alloc_adapter_t<basic_alloc_t>;

		using bp_t = block_pool_t;

		using ad_t = alloc_descr_t;
		using ad_entry_t = alloc_descr_entry_t;
		using ad_addr_cache_t = alloc_descr_addr_cache_t;

		static_assert(has_sysmem_alloc_tag_v<base_t>);

	private:
		using pool_t = pool_entry_t<>;
		using raw_bin_t = raw_entry_t<>;

		using pools_t = impl::pools_t<pool_t, base_t::alloc_pool_first_chunk, base_t::alloc_pool_last_chunk>;
		using raw_bins_t = impl::raw_bins_t<raw_bin_t, base_t::alloc_raw_base_size, base_t::alloc_total_raw_bins>;

	public:
		template<class ... args_t>
		pool_alloc_t(args_t&& ... args) : base_t(std::forward<args_t>(args)...), pools(base_t::get_page_size()) {
			min_pool_alignment = std::min(base_t::get_page_size(), pool_chunk_size((std::size_t)base_t::alloc_pool_first_chunk));
			max_pool_alignment = std::min(base_t::get_page_size(), pool_chunk_size((std::size_t)base_t::alloc_pool_last_chunk));
		}

		pool_alloc_t(const pool_alloc_t&) = delete;
		pool_alloc_t(pool_alloc_t&&) = delete;

		~pool_alloc_t() {
			release_mem();
		}

		pool_alloc_t& operator = (const pool_alloc_t&) = delete;
		pool_alloc_t& operator = (pool_alloc_t&&) = delete; 

		void adopt(pool_alloc_t& another) {
			if (this == &another) {
				return;
			}
			ad_entry.adopt(another.ad_entry);
			addr_cache.adopt(another.addr_cache);
			pools.adopt(another.pools, base_t::alloc_pool_cache_lookups / 2);
			raw_bins.adopt(another.raw_bins, base_t::alloc_raw_cache_lookups / 2);
		}

	public: // for debug
		void release_mem() {
			auto release_func = [&] (void* block, attrs_t offset, void* data, attrs_t size) {
				base_t::deallocate(data, size);
				// free_descr(block, offset); // we can leak descrs here as all blocks will be freed anyways
			};
			pools.release_all(addr_cache, release_func);
			raw_bins.release_all(addr_cache, release_func);
			ad_entry.release_all([&] (void* data, std::size_t size) {
				base_t::deallocate(data, size);
				return true;
			});
		}

	private:
		// returns non-zero on success, returns 0 on failure
		std::size_t adjust_raw_alignment(std::size_t value) {
			if (value == 0) {
				value = base_t::alloc_base_alignment;
			} if (!is_alignment(value)) {
				return 0;
			} if (value <= base_t::get_page_size()) {
				return value;
			} return 0;
		}

		// returns non-zero on success, returns 0 on failure
		std::size_t adjust_pool_alignment(std::size_t value) {
			if (value == 0) {
				value = base_t::alloc_base_alignment;
			} if (!is_alignment(value)) {
				return 0;
			} if (value <= max_pool_alignment) {
				return value;
			} return 0;
		}

		std::size_t fetch_pool_alignment(ad_t* descr) {
			return std::min<std::size_t>(pool_alignment(descr->chunk_size), max_pool_alignment);
		}

	private:
		// returns (memory for block description, offset from primary block)
		[[nodiscard]] std::tuple<void*, attrs_t> alloc_descr() {
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
		bool create_pool(pool_t& pool) {
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
			
			pool.create(addr_cache, ad, offset, pool_size, pool_capacity, pool_data);
			return true;
		}

		template<class entry_t>
		void finish_release(entry_t& entry, ad_t* ad) {
			if (ad) {
				entry.finish_release(addr_cache, ad, [&] (void* block, attrs_t offset, void* data, attrs_t size) {
					base_t::deallocate(data, size);
					free_descr(block, offset);
				});
			}
		}

	private: // alignment must be adjusted beforehand
		[[nodiscard]] void* alloc_pool(pool_t& pool) {
			if (void* ptr = pool.acquire()) {
				return ptr;
			} if (create_pool(pool)) {
				return pool.acquire();
			} return nullptr;
		}

		void free_pool(pool_t& pool, void* ptr) {
			finish_release(pool, pool.release(addr_cache, ptr, base_t::alloc_pool_cache_lookups));
		}

		void free_pool(pool_t& pool, void* ptr, ad_t* ad) {
			finish_release(pool, pool.release(ptr, ad));
		}

		[[nodiscard]] void* alloc_raw(raw_bin_t& bin, std::size_t size, std::size_t alignment) {
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

		[[nodiscard]] void* alloc_raw(std::size_t size, std::size_t alignment) {
			return alloc_raw(*raw_bins.find(size), size, alignment);
		}

		void free_raw(raw_bin_t& bin, void* ptr) {
			finish_release(bin, bin.release(addr_cache, ptr, base_t::alloc_raw_cache_lookups));
		}

		void free_raw(void* ptr, std::size_t size) {
			free_raw(*raw_bins.find(size), ptr);
		}

		void free_raw(raw_bin_t& bin, ad_t* ad) {
			finish_release(bin, bin.release(ad));
		}

		void free_raw(ad_t* ad) {
			free_raw(*raw_bins.find(ad->get_size()), ad);
		}

		[[nodiscard]] ad_t* extract_raw(raw_bin_t& bin, void* ptr) {
			return bin.extract(addr_cache, ptr, base_t::alloc_raw_cache_lookups);
		}

		void put_back_raw(raw_bin_t& bin, ad_t* ad) {
			bin.put_back(addr_cache, ad);
		}

		[[nodiscard]] void* realloc_raw(void* old_ptr, std::size_t old_size, std::size_t alignment, std::size_t new_size) {
			std::size_t old_size_aligned = align_value(old_size, alignment);
			std::size_t new_size_aligned = align_value(new_size, alignment);
			if (old_size_aligned == new_size_aligned) {
				return old_ptr;
			}

			auto old_bin = raw_bins.find(old_size_aligned);
			auto new_bin = raw_bins.find(new_size_aligned);

			ad_t* extracted = extract_raw(*old_bin, old_ptr);
			if (!extracted) {
				return nullptr; // very bad
			} if (alignment != extracted->get_alignment()) {
				return nullptr;
			}

			void* new_memory = base_t::reallocate(extracted->get_data(), old_size_aligned, new_size_aligned);
			if (!new_memory) {
				put_back_raw(*old_bin, extracted);
				return nullptr;
			}
			extracted->set_data(new_memory);
			extracted->set_size(new_size_aligned);
			put_back_raw(*new_bin, extracted);
			return new_memory;
		}

	private:
		[[nodiscard]] void* alloc42(std::size_t size) {
			assert(size != 0);

			return alloc42(size, 0);
		}

		[[nodiscard]] void* alloc42(std::size_t size, std::size_t alignment) {
			assert(size != 0);

			if (std::size_t pool_alignment = adjust_pool_alignment(alignment)) {
				std::size_t size_aligned = align_value(size, pool_alignment);
				if (auto pool = pools.find(size_aligned); pool != pools.end()) {
					return alloc_pool(*pool);
				}
			}

			if (std::size_t raw_alignment = adjust_raw_alignment(alignment)) {
				std::size_t size_aligned = align_value(size, raw_alignment);
				return alloc_raw(size_aligned, raw_alignment);
			}
			
			return nullptr;
		}

		void free42(void* ptr) {
			assert(ptr);

			if (ad_t* ad = addr_cache.find(ptr)) {
				free42(ad, ptr);
				return;
			} std::abort();
		}

		void free42(ad_t* ad, void* ptr) {
			assert(ad);
			assert(ptr);

			using enum block_type_t;

			switch (block_type_t{ad->get_type()}) {
				case Pool: {
					attrs_t chunk_size = pool_chunk_size<attrs_t>(ad->get_chunk_size());
					if (auto pool = pools.find(chunk_size); pool != pools.end()) {
						free_pool(*pool, ptr, ad);
						return;
					} std::abort();
				} case Raw: {
					free_raw(ad);
					return;
				} default: {
					std::abort();
				}
			}
		}

		void free42(void* ptr, std::size_t size, std::size_t alignment) {
			assert(ptr);
			assert(size != 0);

			if (std::size_t pool_alignment = adjust_pool_alignment(alignment)) {
				std::size_t size_aligned = align_value(size, alignment);
				if (auto pool = pools.find(size_aligned); pool != pools.end()) {
					free_pool(*pool, ptr);
					return;
				}
			}

			if (std::size_t raw_alignment = adjust_raw_alignment(alignment)) {
				std::size_t size_aligned = align_value(size, alignment);
				free_raw(ptr, size_aligned);
			}
		}

		[[nodiscard]] void* realloc42(void* old_ptr, std::size_t new_size) {
			assert(old_ptr);
			assert(new_size != 0);

			using enum block_type_t;

			ad_t* ad = addr_cache.find(old_ptr);
			if (!ad) {
				return nullptr;
			}

			std::size_t old_size = 0;
			std::size_t alignment = 0;
			switch (block_type_t{ad->get_type()}) {
				case Pool: {
					old_size = pool_chunk_size(ad->get_chunk_size());
					alignment = fetch_pool_alignment(ad);
					break;
				} case Raw: {
					old_size = ad->get_size();
					alignment = ad->get_alignment();
					break;
				} default: {
					std::abort();
				}
			} return realloc42(old_ptr, old_size, alignment, new_size);
		}

		[[nodiscard]] void* realloc42(void* old_ptr, std::size_t old_size, std::size_t alignment, std::size_t new_size) {
			assert(old_size != 0);
			assert(new_size != 0);

			if (std::size_t pool_alignment = adjust_pool_alignment(alignment)) {
				std::size_t old_size_aligned = align_value(old_size, pool_alignment);
				std::size_t new_size_aligned = align_value(new_size, pool_alignment);

				if (old_size_aligned == new_size_aligned) {
					return old_ptr;
				}

				auto old_pool = pools.find(old_size_aligned);
				auto new_pool = pools.find(new_size_aligned);
				bool old_in_pool = old_pool != pools.end();
				bool new_in_pool = new_pool != pools.end(); 

				// pool-pool transfer
				if (old_in_pool && new_in_pool) {
					if (old_pool != pools.end() && old_pool == new_pool) {
						return old_ptr; // same chunk_size
					}

					void* new_ptr = alloc_pool(*new_pool);
					if (!new_ptr) {
						return nullptr;
					}

					std::memcpy(new_ptr, old_ptr, std::min(old_size, new_size));
					free_pool(*old_pool, old_ptr);
					return new_ptr;
				}

				// pool-raw transfer
				if (old_in_pool) {
					void* new_ptr = alloc_raw(new_size_aligned, pool_alignment);
					if (!new_ptr) {
						return nullptr;
					}

					std::memcpy(new_ptr, old_ptr, std::min(old_size, new_size));
					free_pool(*old_pool, old_ptr);
					return new_ptr;
				}

				// raw-pool transfer
				if (new_in_pool) {
					void* new_ptr = alloc_pool(*new_pool);
					if (!new_ptr) {
						return nullptr;
					}

					std::memcpy(new_ptr, old_ptr, std::min(old_size, new_size));
					free_raw(old_ptr, old_size_aligned);
					return new_ptr;
				}

				// raw-raw transfer
				return realloc_raw(old_ptr, old_size, pool_alignment, new_size);
			}

			if (std::size_t raw_alignment = adjust_raw_alignment(alignment)) {
				return realloc_raw(old_ptr, old_size, raw_alignment, new_size);
			}

			return nullptr;
		}

		// zero_alloc logically has any alignment(you can realloc zero allocation to any alignment)
		// but practically it has minimal possible alignment
		[[nodiscard]] void* zero_alloc() {
			return alloc42(1, min_pool_alignment);
		}

		void free_zero(void* ptr) {
			free42(ptr, 1, min_pool_alignment);
		}

	public: // standart API, do not use extension API to free allocations
		[[nodiscard]] void* malloc(std::size_t size) {
			if (size == 0){
				return zero_alloc();
			} return alloc42(size);
		}

		[[nodiscard]] void* realloc(void* ptr, std::size_t new_size) {
			if (!ptr) {
				return malloc(new_size);
			} if (new_size == 0) {
				free42(ptr);
				return zero_alloc();
			} return realloc42(ptr, new_size);
		}

		void free(void* ptr) {
			if (!ptr) {
				return;
			} free42(ptr);
		}

	public: // extension API
		[[nodiscard]] void* malloc(std::size_t size, std::size_t alignment, flags_t flags = 0) {
			if (size == 0) {
				return zero_alloc();
			} return alloc42(size, alignment);
		}

		// alignment and flags must match alignment and flags of old_ptr
		[[nodiscard]] void* realloc(void* ptr, std::size_t old_size, std::size_t alignment, std::size_t new_size, flags_t flags = 0) {
			if (!ptr) {
				return malloc(new_size, alignment, flags);
			} if (old_size == 0) {
				if (new_size == 0) {
					return ptr;
				}
				free_zero(ptr);
				return malloc(new_size, alignment, flags);
			} if (new_size == 0) {
				free42(ptr, old_size, alignment);
				return zero_alloc();
			} return realloc42(ptr, old_size, alignment, new_size);
		}

		// alignment and flags must be same as used with malloc()
		// you cannot free memory allocated via standart malloc/realloc
		void free(void* ptr, std::size_t size, std::size_t alignment, flags_t flags = 0) {
			if (!ptr) {
				return;
			} if (size == 0) {
				free_zero(ptr);
				return;
			} return free42(ptr, size, alignment);
		}	

	private:
		ad_entry_t ad_entry{};
		ad_addr_cache_t addr_cache{}; // common addr cache for all allocations
		pools_t pools{};
		raw_bins_t raw_bins{};
		std::size_t min_pool_alignment{};
		std::size_t max_pool_alignment{};
	};
}