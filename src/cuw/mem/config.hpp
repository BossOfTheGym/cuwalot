#pragma once

#include "alloc_traits.hpp"

namespace cuw::mem {
	namespace impl {
		struct config_traits_t{};
	}

	struct config_traits_t
		: pool_alloc_traits_t<impl::config_traits_t>
		, cached_alloc_traits_t<impl::config_traits_t>
		, page_alloc_traits_t<impl::config_traits_t> {};
}