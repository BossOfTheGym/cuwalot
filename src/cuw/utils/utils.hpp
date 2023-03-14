#pragma once

namespace cuw {
	template<class value_t, class status_t>
	struct value_status_t {
		value_t value{};
		status_t status{};
	};
}