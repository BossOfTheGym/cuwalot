#include <iomanip>
#include <iostream>

#include <cuw/mem/alloc_descr.hpp>

#include "utils.hpp"

using namespace cuw;

using ad_t = mem::alloc_descr_t;
using ad_addr_cache_t = mem::alloc_descr_addr_cache_t;

namespace {
	struct print_ad_t {
		ad_t* descr{};
	};

	std::ostream& operator << (std::ostream& os, const print_ad_t& print) {
		if (print.descr) {
			return os << "[" << print.descr->get_data() << ":" << print.descr->get_size() << "]";
		} return os << "[null]";
	}

	void check_find(ad_addr_cache_t& cache, void* ptr, ad_t* expected) {
		ad_t* descr = cache.find(ptr);
		if (descr != expected) {
			std::cerr << "expected: " << print_ad_t{expected} << " but got: " << print_ad_t{descr} << std::endl;
			std::abort();
		}
	}

	int test_addr_cache() {
		constexpr std::size_t total_ads = 4;

		alignas(mem::block_align) ad_t descrs[total_ads] = {
			{ .size = 2, .data = (void*)0x2 },
			{ .size = 2, .data = (void*)0x6 },
			{ .size = 2, .data = (void*)0xA },
			{ .size = 2, .data = (void*)0xE },
		};

		ad_addr_cache_t cache;
		for (auto& descr : descrs) {
			cache.insert(&descr);
		}

		for (std::uintptr_t i = 0x0; i <= 0x11; i++) {
			void* ptr = (void*)i;
			ad_t* found = cache.find(ptr);
			std::cout << "ptr: " << ptr << " descr: " << print_ad_t{found} << std::endl;
		} return 0;
	}

	int test_addr_cache_adopt() {
		// TODO
		return 0;
	}
}

int main() {
	if (test_addr_cache()) {
		return -1;
	} if (test_addr_cache_adopt()) {
		return -1;
	} return 0;
}