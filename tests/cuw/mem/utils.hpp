#pragma once

#include <iomanip>
#include <cstring>
#include <iostream>

namespace {
	struct mem_view_t {
		inline static constexpr std::size_t default_lines_per_block = 4;
		inline static constexpr std::size_t default_groups_per_line = 4;
		inline static constexpr std::size_t default_bytes_per_group = 4;

		void* ptr{};
		std::size_t size{};
		std::size_t lines_per_block{default_lines_per_block};
		std::size_t groups_per_line{default_groups_per_line};
		std::size_t bytes_per_group{default_bytes_per_group};
	};

	std::ostream& operator << (std::ostream& os, const mem_view_t& view) {
		auto flags = os.flags();

		std::uint8_t* ptr = (std::uint8_t*)view.ptr;
		std::size_t size = view.size;
		std::size_t lines = 0;

		auto try_print_line = [&] () {
			for (std::size_t i = 0; i < view.groups_per_line && size > 0; i++) {
				for (std::size_t j = 0; j < view.default_bytes_per_group && size > 0; j++) {
					os << std::hex << std::setfill('0') << std::setw(2) << (int)(*ptr);
					++ptr;
					--size;
				} if (size == 0) {
					return;
				} if (i != view.groups_per_line - 1) {
					os << ' ';
				} else {
					os << std::endl;
				}
			} if (size == 0) {
				return;
			}

			++lines;
			if (view.lines_per_block && lines % view.lines_per_block == 0) {
				os << std::endl;
			}
		};

		while (size > 0) {
			try_print_line();
		}

		os.flags(flags);
		return os;
	}
}