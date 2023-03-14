#pragma once

#include <type_traits>

namespace cuw::mem {
	struct mem_alloc_tag_t;
	struct sysmem_alloc_tag_t;

	namespace impl {
		template<class type_t, class tag_t, class = void>
		struct has_tag_t : std::false_type {};

		template<class type_t, class tag_t>
		struct has_tag_t<type_t, tag_t, std::enable_if_t<std::is_same_v<typename type_t::tag_t, tag_t>>> : std::true_type {};

		template<class type_t, class tag_t>
		inline constexpr bool has_tag_v = has_tag_t<type_t, tag_t>::value;
	}

	template<class type_t>
	inline constexpr bool has_mem_alloc_tag_v = impl::has_tag_v<type_t, mem_alloc_tag_t>;

	template<class type_t>
	inline constexpr bool has_sysmem_alloc_tag_v = impl::has_tag_v<type_t, sysmem_alloc_tag_t>;
}