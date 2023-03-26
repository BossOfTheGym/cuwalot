#include <iomanip>
#include <iostream>

#include <cuw/mem/alloc_descr.hpp>

#include "utils.hpp"

using namespace cuw;

using ad_t = mem::alloc_descr_t;
using ad_addr_cache_t = mem::alloc_descr_addr_cache_t;

namespace {
	int test_addr_cache() {
		std::cout << "testing addr cache..." << std::endl;

		constexpr std::size_t total_ads = 4;

		alignas(mem::block_align) ad_t descrs1[total_ads] = {
			{ .size = 2, .data = (void*)0x2},
			{ .size = 2, .data = (void*)0x6},
			{ .size = 2, .data = (void*)0x1A},
			{ .size = 2, .data = (void*)0x1E},
		};

		ad_addr_cache_t cache1;
		for (auto& descr : descrs1) {
			cache1.insert(&descr);
		}

		alignas(mem::block_align) ad_t descrs2[total_ads] = {
			{ .size = 2, .data = (void*)0xA},
			{ .size = 2, .data = (void*)0xE},
			{ .size = 2, .data = (void*)0x12},
			{ .size = 2, .data = (void*)0x16},
		};

		ad_addr_cache_t cache2;
		for (auto& descr : descrs2) {
			cache2.insert(&descr);
		} 

		auto test_cache = [] (auto& cache) {
			for (std::uintptr_t i = 0x0; i < 0x22; i++) {
				void* ptr = (void*)i;
				ad_t* found = cache.find(ptr);
				std::cout << "ptr: " << pretty(ptr) << " descr: " << print_ad_t{found} << std::endl;
			}
		};

		std::cout << "cache1:" << std::endl;
		test_cache(cache1);
		std::cout << std::endl;
		std::cout << "cache2:" << std::endl;
		test_cache(cache2);

		cache1.adopt(cache2);

		std::cout << "cache1:" << std::endl;
		test_cache(cache1);
		std::cout << std::endl;
		std::cout << "cache2:" << std::endl;
		test_cache(cache2);

		std::cout << "testing finished" << std::endl << std::endl;

		return 0;
	}

	int test_alloc_descr_cache() {
		std::cout << "testing alloc_descr_cache..." << std::endl;

		constexpr std::size_t total_ads = 4;
		constexpr std::size_t lookups = 2;

		alignas(mem::block_align) ad_t descrs1[total_ads] = {
			{.size = 2, .data = (void*)0x2},
			{.size = 2, .data = (void*)0x6},
			{.size = 2, .data = (void*)0xA},
			{.size = 2, .data = (void*)0xE},
		};

		mem::alloc_descr_cache_t cache1;
		for (auto& descr : descrs1) {
			cache1.insert(&descr);
		}

		alignas(mem::block_align) ad_t descrs2[total_ads] = {
			{.size = 2, .data = (void*)0x12},
			{.size = 2, .data = (void*)0x16},
			{.size = 2, .data = (void*)0x1A},
			{.size = 2, .data = (void*)0x1E},
		};

		mem::alloc_descr_cache_t cache2;
		for (auto& descr : descrs2) {
			cache2.insert(&descr);
		}

		auto check_find = [&] (auto& cache, std::uintptr_t start, std::uintptr_t end) {
			for (std::uintptr_t i = start; i < end; i++) {
				void* ptr = (void*)i;
				ad_t* found = cache.find(ptr, lookups);
				std::cout << "ptr: " << pretty(ptr) << " found: " << print_ad_t{found} << std::endl;
			}
		};

		std::cout << "*** before adoption ***" << std::endl;
		std::cout << "checking cache1:" << std::endl;
		check_find(cache1, 0x0, 0x22);
		std::cout << std::endl;
		std::cout << "checking cache2:" << std::endl;
		check_find(cache2, 0x0, 0x22);
		std::cout << std::endl;

		cache1.adopt(cache2, lookups / 2);

		std::cout << "*** after adoption ***" << std::endl;
		std::cout << "checking cache1:" << std::endl;
		check_find(cache1, 0x0, 0x22);
		std::cout << std::endl;
		std::cout << "checking cache2:" << std::endl;
		check_find(cache2, 0x0, 0x22);
		std::cout << std::endl;

		cache1.release_all([&] (ad_t* ad) {
			cache2.insert(ad);
			return true;
		});

		std::cout << "*** after release_all ***" << std::endl;
		std::cout << "checking cache1:" << std::endl;
		check_find(cache1, 0x0, 0x22);
		std::cout << std::endl;
		std::cout << "checking cache2:" << std::endl;
		check_find(cache2, 0x0, 0x22);
		std::cout << std::endl;

		std::cout << "testing finished" << std::endl << std::endl;

		return 0;
	}
}

int main() {
	if (test_addr_cache()) {
		return -1;
	} if (test_alloc_descr_cache()) {
		return -1;
	} return 0;
}