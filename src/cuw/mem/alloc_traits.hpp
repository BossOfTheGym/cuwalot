#pragma once

#include "core.hpp"

namespace cuw::mem {
	namespace impl {
		template<class traits_t, class = void>
		struct alloc_page_size_t {
			static constexpr std::size_t value = default_page_size;
		};

		template<class traits_t>
		struct alloc_page_size_t<traits_t,
			std::void_t<enable_option_t<std::size_t, decltype(traits_t::alloc_page_size)>>> {
			static constexpr std::size_t value = traits_t::alloc_page_size;
			static_assert(is_alignment(value));
		};

		template<class traits_t>
		inline constexpr std::size_t alloc_page_size_v= alloc_page_size_t<traits_t>::value;


		template<class traits_t, class = void>
		struct use_resolved_page_size_t {
			static constexpr bool value = default_use_resolved_page_size;
		};

		template<class traits_t>
		struct use_resolved_page_size_t<traits_t,
			std::void_t<enable_option_t<bool, decltype(traits_t::use_resolved_page_size)>>> {
			static constexpr bool value = traits_t::use_resolved_page_size;
		};

		template<class traits_t>
		inline constexpr bool use_resolved_page_size_v = use_resolved_page_size_t<traits_t>::value;


		template<class traits_t, class = void>
		struct alloc_block_pool_size_t {
			static constexpr std::size_t value = default_block_pool_size;
		};

		template<class traits_t>
		struct alloc_block_pool_size_t<traits_t,
			std::void_t<enable_option_t<std::size_t, decltype(traits_t::alloc_block_pool_size)>>> {
			static constexpr std::size_t value = traits_t::alloc_block_pool_size;
			static_assert(value / block_size >= min_pool_blocks);
		};

		template<class traits_t>
		inline constexpr std::size_t alloc_block_pool_size_v = alloc_block_pool_size_t<traits_t>::value;


		template<class traits_t, class = void>
		struct alloc_sysmem_pool_size_t {
			static constexpr std::size_t value = default_block_pool_size;
		};

		template<class traits_t>
		struct alloc_sysmem_pool_size_t<traits_t,
			std::void_t<enable_option_t<std::size_t, decltype(traits_t::alloc_sysmem_pool_size)>>> {
			static constexpr std::size_t value = traits_t::alloc_sysmem_pool_size;
			static_assert(value / block_size >= min_pool_blocks);
		};

		template<class traits_t>
		inline constexpr std::size_t alloc_sysmem_pool_size_v = alloc_sysmem_pool_size_t<traits_t>::value;


		template<class traits_t, class = void>
		struct alloc_min_block_size_t {
			static constexpr std::size_t value = default_min_block_size;
		};

		template<class traits_t>
		struct alloc_min_block_size_t<traits_t,
			std::void_t<enable_option_t<std::size_t, decltype(traits_t::alloc_min_block_size)>>> {
			static constexpr std::size_t value = traits_t::alloc_min_block_size;
			static_assert(value > 0);
		};

		template<class traits_t>
		inline constexpr std::size_t alloc_min_block_size_v = alloc_min_block_size_t<traits_t>::value;


		template<class traits_t, class = void>
		struct alloc_merge_coef_t {
			static constexpr std::size_t value = default_merge_coef;
		};

		template<class traits_t>
		struct alloc_merge_coef_t<traits_t,
			std::void_t<enable_option_t<std::size_t, decltype(traits_t::alloc_merge_coef)>>> {
			static constexpr std::size_t value = traits_t::alloc_merge_coef;
		};

		template<class traits_t>
		inline constexpr std::size_t alloc_merge_coef_v = alloc_merge_coef_t<traits_t>::value;


		template<class traits_t, class = void>
		struct alloc_min_pool_power_t {
			static constexpr attrs_t value = default_min_pool_power;
		};

		template<class traits_t>
		struct alloc_min_pool_power_t<traits_t,
			std::void_t<enable_option_t<attrs_t, decltype(traits_t::alloc_min_pool_power)>>> {
			static constexpr attrs_t value = traits_t::alloc_min_pool_power;
			static_assert(value > 0);
		};

		template<class traits_t>
		inline constexpr std::size_t alloc_min_pool_power_v = alloc_min_pool_power_t<traits_t>::value; 


		template<class traits_t>
		struct alloc_min_pool_size_t {
		private:
			static constexpr attrs_t _alloc_min_pool_power = alloc_min_pool_power_v<traits_t>;
			static constexpr std::size_t _alloc_page_size = alloc_page_size_v<traits_t>;
		public:
			static constexpr attrs_t value = (attrs_t)1 << _alloc_min_pool_power; 
			static_assert(value >= _alloc_page_size);
		};

		template<class traits_t>
		inline constexpr attrs_t alloc_min_pool_size_v = alloc_min_pool_size_t<traits_t>::value;


		template<class traits_t, class = void>
		struct alloc_max_pool_power_t {
			static constexpr attrs_t value = default_max_pool_power;
		};

		template<class traits_t>
		struct alloc_max_pool_power_t<traits_t,
			std::void_t<enable_option_t<attrs_t, decltype(traits_t::alloc_max_pool_power)>>> {
		private:
			static constexpr attrs_t _alloc_min_pool_power = alloc_min_pool_power_v<traits_t>;
		public:
			static constexpr attrs_t value = traits_t::alloc_max_pool_power;
			static_assert(_alloc_min_pool_power <= value);
		};

		template<class traits_t>
		inline constexpr attrs_t alloc_max_pool_power_v = alloc_max_pool_power_t<traits_t>::value;


		template<class traits_t>
		struct alloc_max_pool_size_t {
		private:
			static constexpr attrs_t _alloc_max_pool_power = alloc_max_pool_power_v<traits_t>;
		public:
			static constexpr attrs_t value = (attrs_t)1 << _alloc_max_pool_power;
		};

		template<class traits_t>
		inline constexpr attrs_t alloc_max_pool_size_v = alloc_max_pool_size_t<traits_t>::value;


		template<class traits_t, class = void>
		struct alloc_pool_cache_lookups_t {
			static constexpr int value = default_pool_cache_lookups;
		};

		template<class traits_t>
		struct alloc_pool_cache_lookups_t<traits_t,
			std::void_t<enable_option_t<int, decltype(traits_t::alloc_pool_cache_lookups)>>> {
			static constexpr int value = traits_t::alloc_pool_cache_lookups;
		};

		template<class traits_t>
		inline constexpr int alloc_pool_cache_lookups_v = alloc_pool_cache_lookups_t<traits_t>::value;


		template<class traits_t, class = void>
		struct alloc_raw_cache_lookups_t {
			static constexpr int value = default_raw_cache_lookups;
		};

		template<class traits_t>
		struct alloc_raw_cache_lookups_t<traits_t,
			std::void_t<enable_option_t<int, decltype(traits_t::alloc_raw_cache_lookups)>>> {
			static constexpr int value = traits_t::alloc_raw_cache_lookups;
			static_assert(value % 2 == 0); // why?
		};

		template<class traits_t>
		inline constexpr int alloc_raw_cache_lookups_v = alloc_raw_cache_lookups_t<traits_t>::value;


		template<class traits_t, class = void>
		struct alloc_basic_alignment_t {
			static constexpr std::size_t value = default_basic_alignment;
		};

		template<class traits_t>
		struct alloc_basic_alignment_t<traits_t,
			std::void_t<enable_option_t<std::size_t, decltype(traits_t::alloc_basic_alignment)>>> {
			static constexpr std::size_t value = traits_t::alloc_basic_alignment;
		};

		template<class traits_t>
		inline constexpr std::size_t alloc_basic_alignment_v = alloc_basic_alignment_t<traits_t>::value; 


		template<class traits_t, class = void>
		struct alloc_cache_slots_t {
			static constexpr std::size_t value = default_cache_slots;
		};

		template<class traits_t>
		struct alloc_cache_slots_t<traits_t,
			std::void_t<enable_option_t<std::size_t, decltype(traits_t::alloc_cache_slots)>>> {
			static constexpr std::size_t value = traits_t::alloc_cache_slots;
			static_assert(value > 0);
		};

		template<class traits_t>
		inline constexpr std::size_t alloc_cache_slots_v = alloc_cache_slots_t<traits_t>::value;


		template<class traits_t, class = void>
		struct alloc_min_slot_size_t {
			static constexpr std::size_t value = default_min_slot_size;
		};

		template<class traits_t>
		struct alloc_min_slot_size_t<traits_t,
			std::void_t<enable_option_t<std::size_t, decltype(traits_t::alloc_min_slot_size)>>> {
		private:
			static constexpr std::size_t _alloc_page_size = alloc_page_size_v<traits_t>; 
		public:
			static constexpr std::size_t value = traits_t::alloc_min_slot_size;
			static_assert(is_aligned(value, _alloc_page_size));
		};

		template<class traits_t>
		inline constexpr std::size_t alloc_min_slot_size_v = alloc_min_slot_size_t<traits_t>::value;


		template<class traits_t, class = void>
		struct alloc_max_slot_size_t {
			static constexpr std::size_t value = default_max_slot_size;
		};

		template<class traits_t>
		struct alloc_max_slot_size_t<traits_t,
			std::void_t<enable_option_t<std::size_t, decltype(traits_t::alloc_max_slot_size)>>> {
		private:
			static constexpr std::size_t _alloc_min_slot_size = alloc_min_slot_size_v<traits_t>;
			static constexpr std::size_t _alloc_page_size = alloc_page_size_v<traits_t>;
		public:
			static constexpr std::size_t value = traits_t::alloc_max_slot_size;
			static_assert(value >= _alloc_min_slot_size);
			static_assert(is_aligned(value, _alloc_page_size));
		};

		template<class traits_t>
		inline constexpr std::size_t alloc_max_slot_size_v = alloc_max_slot_size_t<traits_t>::value;


		template<class traits_t>
		struct alloc_max_cache_size_t {
		private:
			static constexpr std::size_t _alloc_cache_slots = alloc_cache_slots_v<traits_t>;
			static constexpr std::size_t _alloc_max_slot_size = alloc_max_slot_size_v<traits_t>; 
		public:
			static constexpr std::size_t value = _alloc_cache_slots * _alloc_max_slot_size;
		};

		template<class traits_t>
		inline constexpr std::size_t alloc_max_cache_size_v = alloc_max_cache_size_t<traits_t>::value;


		template<class traits_t, class = void>
		struct use_alloc_cache_t {
			static constexpr bool value = default_use_alloc_cache;
		};

		template<class traits_t>
		struct use_alloc_cache_t<traits_t,
			std::void_t<enable_option_t<bool, decltype(traits_t::use_alloc_cache)>>> {
			static constexpr bool value = traits_t::use_alloc_cache;
		};

		template<class traits_t>
		inline constexpr bool use_alloc_cache_v = use_alloc_cache_t<traits_t>::value;


		template<class traits_t, class = void>
		struct use_locking_t {
			static constexpr bool value = default_use_locking;
		};

		template<class traits_t>
		struct use_locking_t<traits_t,
			std::void_t<enable_option_t<bool, decltype(traits_t::use_locking)>>> {
			static constexpr bool value = traits_t::use_locking;
		};

		template<class traits_t>
		inline constexpr bool use_locking_v = use_locking_t<traits_t>::value;


		template<class traits_t, class = void>
		struct use_dirty_optimization_hacks_t {
			static constexpr bool value = default_use_dirty_optimization_hacks;
		};

		template<class traits_t>
		struct use_dirty_optimization_hacks_t<traits_t,
			std::void_t<enable_option_t<bool, decltype(traits_t::use_dirty_optimization_hacks)>>> {
			static constexpr bool value = traits_t::use_dirty_optimization_hacks;
		};

		template<class traits_t>
		inline constexpr bool use_dirty_optimization_hacks_v = use_dirty_optimization_hacks_t<traits_t>::value;


		template<class traits_t>
		struct check_alloc_cache_t {
		private:
			static constexpr bool _use_alloc_cache = use_alloc_cache_v<traits_t>;
			static constexpr std::size_t _alloc_min_slot_size = alloc_min_slot_size_v<traits_t>;
			static constexpr std::size_t _alloc_min_pool_size = alloc_min_pool_size_v<traits_t>;
		public:
			static constexpr bool check = _alloc_min_slot_size >= _alloc_min_pool_size;
		};

		template<class traits_t>
		inline constexpr bool check_alloc_cache_v = check_alloc_cache_t<traits_t>::check;


		// TODO : rework
		template<class traits_t, class = void>
		struct alloc_pool_specs_t {
			static constexpr pool_chunk_size_t alloc_pool_first_chunk = default_pool_first_chunk;
			static constexpr pool_chunk_size_t alloc_pool_last_chunk = default_pool_last_chunk;
		};

		template<class traits_t>
		struct alloc_pool_specs_t<traits_t,
			std::void_t<
				enable_option_t<pool_chunk_size_t, decltype(traits_t::alloc_pool_first_chunk)>,
				enable_option_t<pool_chunk_size_t, decltype(traits_t::alloc_pool_last_chunk)>
			>
		> {
			static constexpr pool_chunk_size_t alloc_pool_first_chunk = traits_t::alloc_pool_first_chunk;
			static constexpr pool_chunk_size_t alloc_pool_last_chunk = traits_t::alloc_pool_last_chunk;
			static_assert((attrs_t)alloc_pool_first_chunk <= (attrs_t)alloc_pool_last_chunk);
		};

		// TODO : rework
		template<class traits_t, class = void>
		struct alloc_raw_bins_specs_t {
		private:
			static constexpr pool_chunk_size_t _alloc_pool_last_chunk = alloc_pool_specs_t<traits_t>::alloc_pool_last_chunk;
		public:
			static constexpr attrs_t alloc_raw_base_size = pool_chunk_size((attrs_t)_alloc_pool_last_chunk);
			static constexpr attrs_t alloc_total_raw_bins = default_total_raw_bins;
		};

		template<class traits_t>
		struct alloc_raw_bins_specs_t<traits_t,
			std::void_t<
				enable_option_t<attrs_t, decltype(traits_t::alloc_raw_base_size)>,
				enable_option_t<attrs_t, decltype(traits_t::alloc_total_raw_bins)>
			>
		> {
		private:
			static constexpr pool_chunk_size_t _alloc_pool_last_chunk = alloc_pool_specs_t<traits_t>::alloc_pool_last_chunk;
		public:
			static constexpr attrs_t alloc_raw_base_size = traits_t::alloc_raw_base_size;
			static constexpr attrs_t alloc_total_raw_bins = traits_t::alloc_total_raw_bins;
			static_assert(alloc_total_raw_bins != 0);
			static_assert(alloc_raw_base_size >= pool_chunk_size((attrs_t)_alloc_pool_last_chunk));
		};
	}

	struct empty_traits_t {};

	template<class traits_t>
	struct page_alloc_traits_t {
		static constexpr bool use_resolved_page_size = impl::use_resolved_page_size_v<traits_t>;
		static constexpr bool use_dirty_optimization_hacks = impl::use_dirty_optimization_hacks_v<traits_t>;

		static constexpr std::size_t alloc_page_size = impl::alloc_page_size_v<traits_t>;
		static constexpr std::size_t alloc_block_pool_size = impl::alloc_block_pool_size_v<traits_t>;
		static constexpr std::size_t alloc_sysmem_pool_size = impl::alloc_sysmem_pool_size_v<traits_t>;
		static constexpr std::size_t alloc_min_block_size = impl::alloc_min_block_size_v<traits_t>;
		static constexpr std::size_t alloc_merge_coef = impl::alloc_merge_coef_v<traits_t>; // unused
	};

	template<class traits_t>
	struct cached_alloc_traits_t {
		static constexpr std::size_t alloc_cache_slots = impl::alloc_cache_slots_v<traits_t>;
		static constexpr std::size_t alloc_min_slot_size = impl::alloc_min_slot_size_v<traits_t>;
		static constexpr std::size_t alloc_max_slot_size = impl::alloc_max_slot_size_v<traits_t>;
		static constexpr std::size_t alloc_max_cache_size = impl::alloc_max_cache_size_v<traits_t>;
	};

	template<class traits_t>
	struct pool_alloc_traits_t {
		static constexpr std::size_t alloc_min_pool_power = impl::alloc_min_pool_power_v<traits_t>;
		static constexpr std::size_t alloc_max_pool_power = impl::alloc_max_pool_power_v<traits_t>;
		static constexpr std::size_t alloc_min_pool_size = impl::alloc_min_pool_size_v<traits_t>;
		static constexpr std::size_t alloc_max_pool_size = impl::alloc_max_pool_size_v<traits_t>;
		static constexpr std::size_t alloc_basic_alignment = impl::alloc_basic_alignment_v<traits_t>;

		static constexpr int alloc_pool_cache_lookups = impl::alloc_pool_cache_lookups_v<traits_t>;
		static constexpr int alloc_raw_cache_lookups = impl::alloc_raw_cache_lookups_v<traits_t>;

		static constexpr bool use_alloc_cache = impl::use_alloc_cache_v<traits_t>;
		static constexpr bool use_locking = impl::use_locking_v<traits_t>;

		static constexpr pool_chunk_size_t alloc_pool_first_chunk = impl::alloc_pool_specs_t<traits_t>::alloc_pool_first_chunk; // TODO
		static constexpr pool_chunk_size_t alloc_pool_last_chunk = impl::alloc_pool_specs_t<traits_t>::alloc_pool_last_chunk; // TODO

		static constexpr std::size_t alloc_raw_base_size = impl::alloc_raw_bins_specs_t<traits_t>::alloc_raw_base_size; // TODO
		static constexpr std::size_t alloc_total_raw_bins = impl::alloc_raw_bins_specs_t<traits_t>::alloc_total_raw_bins; // TODO

		static_assert(impl::check_alloc_cache_v<traits_t>);
	};
}