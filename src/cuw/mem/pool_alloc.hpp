#pragma once

#include "core.hpp"
#include "alloc_tag.hpp"
#include "block_pool.hpp"
#include "alloc_traits.hpp"
#include "cached_alloc.hpp"
#include "alloc_entries.hpp"

namespace cuw::mem {
	using alloc_descr_entry_t = block_pool_entry_t;

	namespace impl {
		// TODO : make use of allocate_ext
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

			std::size_t get_count() const {
				return total_pools;
			}

			pool_type_t& get(int i) {
				assert(i >= 0 && i < total_pools);
				return pools[i];
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

		template<class bin_type_t, attrs_t total_bins>
		class raw_bins_t {
		public:
			using ad_t = alloc_descr_t;
			using ad_addr_cache_t = alloc_descr_addr_cache_t;

			raw_bins_t(std::size_t base_size) {
				for (attrs_t i = 1; i < total_bins; i++) {
					size_limits[i - 1] = base_size << i;
				}
			}

			std::size_t get_count() const {
				return total_bins;
			}

			bin_type_t& get(int i) {
				assert(i >= 0 && i < total_bins);
				return bins[i];
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

	template<class basic_alloc_t>
	using pool_alloc_adapter_t = impl::pool_alloc_adapter_t<basic_alloc_t, basic_alloc_t::use_alloc_cache>;

	template<class basic_alloc_t>
	class pool_alloc_t : public pool_alloc_adapter_t<basic_alloc_t> {
	public:
		using tag_t = mem_alloc_tag_t;

		using base_t = pool_alloc_adapter_t<basic_alloc_t>;

		using bp_t = block_pool_t;

		using ad_t = alloc_descr_t;
		using ad_state_t = alloc_descr_state_t;
		using ad_entry_t = alloc_descr_entry_t;
		using ad_addr_cache_t = alloc_descr_addr_cache_t;

		static_assert(has_sysmem_alloc_tag_v<base_t>);

	private:
		using pool_t = pool_entry_t;
		using raw_bin_t = raw_entry_t;

		using pools_t = impl::pools_t<pool_t, base_t::alloc_pool_first_chunk, base_t::alloc_pool_last_chunk>;
		using raw_bins_t = impl::raw_bins_t<raw_bin_t, base_t::alloc_total_raw_bins>;

	public:
		template<class ... args_t>
		pool_alloc_t(args_t&& ... args)
			: base_t(std::forward<args_t>(args)...)
			, pools(base_t::get_page_size())
			, raw_bins(base_t::get_page_size()) {
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

	private:
		void adopt_pools_n_bins(pool_alloc_t& another) {
			ad_entry.adopt(another.ad_entry);
			
			pools_t tmp_pools(base_t::get_page_size());
			for (int i = 0; i < tmp_pools.get_count(); i++) {
				auto& tmp_pool = tmp_pools.get(i);
				auto& pool = another.pools.get(i);
				pool.traverse([&] (ad_t* ad) {
					ad_t* new_descr = realloc_descr(ad);
					tmp_pool.insert_back(new_descr);
					addr_cache.insert(new_descr);
				});
			}
			another.pools.reset();
			pools.adopt(tmp_pools, base_t::alloc_pool_cache_lookups / 2);

			raw_bins_t tmp_raw_bins(base_t::get_page_size());
			for (int i = 0; i < tmp_raw_bins.get_count(); i++) {
				auto& tmp_raw_bin = tmp_raw_bins.get(i);
				auto& bin = another.raw_bins.get(i);
				bin.traverse([&] (ad_t* ad) {
					ad_t* new_descr = realloc_descr(ad);
					tmp_raw_bin.insert_back(new_descr);
					addr_cache.insert(new_descr);
				});
			}
			another.raw_bins.reset();
			raw_bins.adopt(tmp_raw_bins, base_t::alloc_raw_cache_lookups / 2);

			another.addr_cache.reset();
		}

	public:
		void adopt(pool_alloc_t& another) {
			if (this == &another) {
				return;
			}

			std::size_t this_descr_count = ad_entry.get_count();
			std::size_t another_descr_count = another.ad_entry.get_count();
			std::size_t total_count = this_descr_count + another_descr_count;
			std::size_t total_capacity = ad_entry.get_total_capacity() + another.ad_entry.get_total_capacity();
			if (total_capacity / total_count >= 2) {
				if (this_descr_count > another_descr_count) {
					adopt_pools_n_bins(another);
				} else {
					another.adopt_pools_n_bins(*this);
					addr_cache = another.addr_cache;
					another.addr_cache.reset();
					ad_entry = std::move(another.ad_entry);
					pools = std::move(another.pools);
					raw_bins= std::move(another.raw_bins);
				}
			} else {
				ad_entry.adopt(another.ad_entry);
				addr_cache.adopt(another.addr_cache);
				pools.adopt(another.pools, base_t::alloc_pool_cache_lookups / 2);
				raw_bins.adopt(another.raw_bins, base_t::alloc_raw_cache_lookups / 2);
			}
		}

	public: // for debug
		void release_mem() {
			auto release_func = [&] (void* block, attrs_t offset, void* data, attrs_t size) {
				base_t::deallocate(data, size); // we can leak descrs here as all blocks will be freed anyways
				return true;			
			};
			pools.release_all(release_func);
			raw_bins.release_all(release_func);
			ad_entry.release_all([&] (void* data, std::size_t size) {
				base_t::deallocate(data, size);
				return true;
			});
			addr_cache.reset();
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
		[[nodiscard]] block_info_t alloc_descr() {
			if (auto [ptr, offset] = ad_entry.acquire(); ptr) {
				return {ptr, offset};
			}

			std::size_t pool_size = base_t::alloc_block_pool_size;
			if (void* pool_mem = base_t::allocate(pool_size)) {
				ad_entry.create_pool(pool_mem, pool_size);
				return ad_entry.acquire();
			} return {nullptr, block_pool_head_empty};
		}

		// index & cache data is invalidated
		[[nodiscard]] ad_t* realloc_descr(ad_t* descr) {
			assert(descr);

			ad_state_t old_state = descr->get_state();
			ad_entry.release(descr, descr->get_offset(), block_pool_release_mode_t::NoReinsertFree);

			auto [new_block, new_offset] = ad_entry.acquire();

			assert(new_block);

			ad_t* new_descr = (ad_t*)new_block;
			new_descr->set_state(old_state);
			new_descr->set_offset(new_offset);
			return new_descr;
		}

		// deallocates description, does not free associated memory
		void free_descr(void* descr, attrs_t offset, block_pool_release_mode_t mode = block_pool_release_mode_t::ReinsertFree) {
			if (bp_t* released = ad_entry.release(descr, offset, mode)) {
				ad_entry.finish_release(released, [&] (void* data, std::size_t size) {
					base_t::deallocate(data, size);
					return true;
				});
			}
		}

	private:
		ad_t* create_pool(pool_t& pool) {
			auto [ad, offset] = alloc_descr();
			if (!ad) {
				return nullptr;
			}

			// TODO : calculate it here, remove get_next_pool_params
			auto [pool_size, pool_capacity] = pool.get_next_pool_params(base_t::alloc_min_pool_power, base_t::alloc_max_pool_power);

			void* pool_data = base_t::allocate(pool_size);
			if (!pool_data) {
				free_descr(ad, offset);
				return nullptr;
			}
			
			ad_t* pool_ad = pool.create(ad, offset, pool_size, pool_capacity, pool_data);
			if (pool_ad) {
				addr_cache.insert(pool_ad);
			} return pool_ad;
		}

		template<class entry_t>
		void finish_release(entry_t& entry, ad_t* ad) {
			addr_cache.erase(ad);
			entry.finish_release(ad);
			base_t::deallocate(ad->get_data(), ad->get_size());
			free_descr(ad, ad->get_offset());
		}

	private: // alignment must be adjusted beforehand
		[[nodiscard]] void* alloc_pool(pool_t& pool) {
			if (void* ptr = pool.acquire()) {
				return ptr;
			} if (create_pool(pool)) {
				return pool.acquire();
			} return nullptr;
		}

		bool free_pool(pool_t& pool, void* ptr) {
			if (auto [ad, ptr_released] = pool.release(addr_cache, ptr, base_t::alloc_pool_cache_lookups); ptr_released) {
				if (ad) {
					finish_release(pool, ad);
				} return true;
			} return false;
		}

		bool free_pool(pool_t& pool, void* ptr, ad_t* ad) {
			if (auto [ad, ptr_released] = pool.release(ptr, ad); ptr_released) {
				if (ad) {
					finish_release(pool, ad);
				} return true;
			} return false;
		}

		[[nodiscard]] void* alloc_raw(raw_bin_t& bin, std::size_t size, std::size_t alignment) {
			auto [ad_mem, offset] = alloc_descr();
			if (!ad_mem) {
				return nullptr;
			}

			std::size_t size_aligned = align_value(size, alignment);
			void* data = base_t::allocate(size_aligned);
			if (!data) {
				free_descr(ad_mem, offset);
				return nullptr;
			}

			ad_t* ad = bin.create(ad_mem, offset, size, alignment, data);
			if (ad) {
				addr_cache.insert(ad);
			} return data;
		}

		[[nodiscard]] void* alloc_raw(std::size_t size, std::size_t alignment) {
			return alloc_raw(*raw_bins.find(size), size, alignment);
		}

		bool free_raw(raw_bin_t& bin, ad_t* ad) {
			if (ad) {
				finish_release(bin, bin.release(ad));
				return true;
			} return false;
		}

		bool free_raw(ad_t* ad) {
			return free_raw(*raw_bins.find(ad->get_size()), ad);
		}

		bool free_raw(raw_bin_t& bin, void* ptr) {
			if (ad_t* ad = bin.release(addr_cache, ptr, base_t::alloc_raw_cache_lookups)) {
				finish_release(bin, ad);
				return true;
			} return false;
		}

		bool free_raw(void* ptr, std::size_t size) {
			return free_raw(*raw_bins.find(size), ptr);
		}

		[[nodiscard]] ad_t* extract_raw(raw_bin_t& bin, void* ptr) {
			return bin.extract(addr_cache, ptr, base_t::alloc_raw_cache_lookups);
		}

		void put_back_raw(raw_bin_t& bin, ad_t* ad) {
			bin.put_back(ad);
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

			// we fall here when either alignment or size is too big or both
			if (std::size_t raw_alignment = adjust_raw_alignment(alignment)) {
				std::size_t size_aligned = align_value(size, raw_alignment);
				return alloc_raw(size_aligned, raw_alignment);
			}
			
			return nullptr;
		}

		bool free42(void* ptr) {
			assert(ptr);

			if (ad_t* ad = addr_cache.find(ptr)) {
				free42(ad, ptr);
				return true;
			} return false;
		}

		bool free42(ad_t* ad, void* ptr) {
			assert(ad);
			assert(ptr);

			using enum block_type_t;

			switch (block_type_t{ad->get_type()}) {
				case Pool: {
					attrs_t chunk_size = pool_chunk_size<attrs_t>(ad->get_chunk_size());
					if (auto pool = pools.find(chunk_size); pool != pools.end()) {
						return free_pool(*pool, ptr, ad);
					} return false;
				} case Raw: {
					return free_raw(ad);
				} default: {
					return false;
				}
			}
		}

		// we fall here when either alignment or size is too big or both
		bool free42(void* ptr, std::size_t size, std::size_t alignment) {
			assert(ptr);
			assert(size != 0);

			if (std::size_t pool_alignment = adjust_pool_alignment(alignment)) {
				std::size_t size_aligned = align_value(size, pool_alignment);
				if (auto pool = pools.find(size_aligned); pool != pools.end()) {
					return free_pool(*pool, ptr);
				}
			}

			if (std::size_t raw_alignment = adjust_raw_alignment(alignment)) {
				std::size_t size_aligned = align_value(size, raw_alignment);
				return free_raw(ptr, size_aligned);
			}

			return false;
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

		bool free_zero(void* ptr) {
			return free42(ptr, 1, min_pool_alignment);
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

		bool free(void* ptr) {
			if (!ptr) {
				return true;
			} return free42(ptr);
		}

	public: // extension API
		[[nodiscard]] void* malloc(std::size_t size, std::size_t alignment, flags_t flags = 0) {
			if (size == 0) {
				return zero_alloc();
			} return alloc42(size, alignment);
		}

		// alignment and flags must match alignment and flags of old_ptr
		[[nodiscard]] void* realloc(void* ptr, std::size_t old_size, std::size_t new_size, std::size_t alignment, flags_t flags = 0) {
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
		bool free(void* ptr, std::size_t size, std::size_t alignment, flags_t flags = 0) {
			if (!ptr) {
				return true;
			} if (size == 0) {
				return free_zero(ptr);
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