#pragma once

#include <random>
#include <iomanip>
#include <cstring>
#include <iostream>

#include <cuw/mem/page_alloc.hpp>
#include <cuw/mem/alloc_descr.hpp>

using namespace cuw;

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

	struct print_range_t {
		void* ptr{};
		std::size_t size{};
	};

	std::ostream& operator << (std::ostream& os, const print_range_t& print) {
		return os << "[" << pretty(print.ptr) << "-" << pretty(advance_ptr(print.ptr, print.size)) << ":" << print.size << "]";
	}

	struct print_ad_t {
		cuw::mem::alloc_descr_t* descr{};
	};

	std::ostream& operator << (std::ostream& os, const print_ad_t& print) {
		if (print.descr) {
			return os << print_range_t{print.descr->get_data(), print.descr->get_size()};
		} return os << "[null descr]";
	}

	template<class fbd_t>
	struct fbd_info_t {
		friend std::ostream& operator << (std::ostream& os, const fbd_info_t& info) {
			return os << " fbd: " << (void*)info.fbd
				<< " off: " << info.fbd->offset
				<< " size: " << info.fbd->size
				<< " data: " << info.fbd->data;
		}

		fbd_t* fbd{};
	};

	template<class page_alloc_t>
	struct addr_index_info_t {
		friend std::ostream& operator << (std::ostream& os, const addr_index_info_t& nodes) {
			for (auto index : nodes.alloc.get_addr_index()) {
				os << fbd_info_t{mem::free_block_descr_t::addr_index_to_descr(index)} << std::endl;
			} return os;
		}

		page_alloc_t& alloc;
	};

	template<class page_alloc_t>
	struct size_index_info_t {
		friend std::ostream& operator << (std::ostream& os, const size_index_info_t& nodes) {
			for (auto index : nodes.alloc.get_size_index()) {
				os << fbd_info_t{mem::free_block_descr_t::size_index_to_descr(index)} << std::endl;
			} return os;
		}

		page_alloc_t& alloc;
	};

	template<class page_alloc_t>
	struct page_alloc_info_t {
		friend std::ostream& operator << (std::ostream& os, const page_alloc_info_t& info) {
			return os << "addr index:" << std::endl << addr_index_info_t{info.alloc} << std::endl
				<< "size index:" << std::endl << size_index_info_t{info.alloc} << std::endl;
		}

		page_alloc_t& alloc;
	};

	class int_gen_t {
	public:
		int_gen_t(int seed) : rgen(seed) {}

		// generate number from [a, b)
		int gen(int a, int b) {
			int value = rgen();
			return value % (b - a) + a;
		}

		// generate number from [0, a)
		int gen(int a) {
			int value = rgen();
			return value % a;
		}

	private:
		std::minstd_rand0 rgen;
	};

	// size can be zero, is size is zero then ptr can be nullptr
	void memset_deadbeef(void* ptr, std::size_t size) {
		const char* deadbeef = "\xde\xad\xbe\xef";
		while (size >= 4) {
			std::memcpy(ptr, deadbeef, 4);
			ptr = (char*)ptr + 4;
			size -= 4;
		} if (size > 0) {
			std::memcpy(ptr, deadbeef, size);
		}
	}
}