#pragma once

#include <iomanip>
#include <cstring>
#include <iostream>

namespace {
	struct ios_state_guard_t {
		ios_state_guard_t(std::ostream& _os)
			: os{_os}, precision{os.precision()}, width{os.width()}, iostate{os.rdstate()}, flags{os.flags()} {}

		~ios_state_guard_t() {
			os.precision(precision);
			os.width(width);
			os.setstate(iostate);
			os.flags(flags);
		}

		std::ostream& os;
		std::streamsize precision;
		std::streamsize width;
		std::ios_base::iostate iostate;
		std::ios_base::fmtflags flags;
	};

	template<class type_t>
	struct pretty_t {
		type_t data;
	};

	template<>
	struct pretty_t<void*> {
		void* ptr{};
	};

	template<class type_t>
	auto pretty(type_t data) {
		return pretty_t{data};
	}

	std::ostream& operator << (std::ostream& os, const pretty_t<void*>& data) {
		ios_state_guard_t guard{os};
		return os << std::setw(sizeof(void*) * 2 + 2) << std::setfill('0') << std::internal << std::showbase << std::hex << (std::uintptr_t)data.ptr;
	}

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
		ios_state_guard_t guard{os};

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
		} return os;
	}
}