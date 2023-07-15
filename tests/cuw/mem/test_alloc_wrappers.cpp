#include <vector>
#include <cstring>
#include <iostream>

#include <cuw/mem/core.hpp>
#include <cuw/mem/alloc_descr.hpp>
#include <cuw/mem/alloc_entries.hpp>
#include <cuw/mem/alloc_wrappers.hpp>

#include "utils.hpp"

using namespace cuw;

namespace {
	using ad_t = mem::alloc_descr_t;
	using attrs_t = mem::attrs_t;

	class test_ad_t {
	public:
		test_ad_t(attrs_t type, attrs_t chunk_size_log2, std::size_t chunk_size, std::size_t size, void* data) {
			std::size_t capacity = size / chunk_size;
			descr = ad_t {
				.size = size,
				.type = type, .chunk_size = chunk_size_log2, .capacity = capacity, .head = mem::alloc_descr_head_empty,
				.data = data
			};
		}

		~test_ad_t() {}

		test_ad_t(test_ad_t&& another) noexcept : descr{std::exchange(another.descr, ad_t{})} {}
		test_ad_t(const test_ad_t&) = delete;

		test_ad_t& operator = (test_ad_t&& another) noexcept {
			std::swap(descr, another.descr);
			return *this;
		}

		test_ad_t& operator = (const test_ad_t&) = delete;

		operator ad_t* () {
			return get();
		}

		ad_t* get() {
			return &descr;
		}

	private:
		ad_t descr{};
	};

	test_ad_t create_pool(mem::pool_chunk_size_t chunk_size, std::size_t size, void* data) {
		return test_ad_t((attrs_t)mem::block_type_t::Pool, (attrs_t)chunk_size, mem::pool_chunk_size((attrs_t)chunk_size), size, data);
	}

	template<attrs_t chunk_size_log2, attrs_t total_chunks>
	int test_pool_wrapper() {
		constexpr attrs_t chunk_align = mem::value_to_pow2(chunk_size_log2);
		constexpr attrs_t chunk_size = mem::value_to_pow2(chunk_size_log2);
		constexpr attrs_t total_size = total_chunks * chunk_size;
		constexpr attrs_t lines_per_block = chunk_size / (mem_view_t::default_groups_per_line * mem_view_t::default_bytes_per_group);

		alignas(chunk_align) std::uint8_t data[total_size] = {};

		test_ad_t test_ad = create_pool(chunk_enum, total_size, data);
		mem::basic_pool_wrapper_t wrapper(test_ad, (attrs_t)chunk_enum, chunk_align);

		void* allocated[total_chunks] = {};

		auto allocate = [&] (auto& chunk, int value) {
			chunk = wrapper.acquire_chunk();
			if (!chunk) {
				std::cerr << "something went wrong..." << std::endl;
				std::abort();
			}

			std::memset(chunk, value, chunk_size);
			std::cout << "allocated: " << (void*)chunk << std::endl;
		};

		auto deallocate = [&] (auto& chunk, int value) {
			std::uint8_t cmp_chunk[chunk_size] = {};
			std::memset(cmp_chunk, value, chunk_size);
			if (std::memcmp(chunk, cmp_chunk, chunk_size)) {
				std::cerr << "invalid check value" << std::endl;
				std::abort();
			}

			wrapper.release_chunk(chunk);
			std::cout << "deallocated: " << (void*)chunk << std::endl;
			chunk = nullptr;
		};

		auto check_invalid_allocation = [&] () {
			if (void* chunk = wrapper.acquire_chunk()) {
				std::cerr << "wrong!" << std::endl;
				std::abort();
			}
		};

		std::cout << "testing pool wrapper with chunk_size = " << chunk_size << std::endl;

		std::cout << "allocating all..." << std::endl;
		for (int i = 0; i < total_chunks; i++) {
			allocate(allocated[i], i);
		}

		check_invalid_allocation();

		std::cout << "deallocating all.." << std::endl;
		for (int i = 0; i < total_chunks; i++) {
			deallocate(allocated[i], i);
		}
		std::cout << std::endl;

		std::cout << "allocating all..." << std::endl;
		for (int i = 0; i < total_chunks; i++) {
			allocate(allocated[i], i);
		}
		std::cout << std::endl;

		check_invalid_allocation();

		std::cout << "deallocating even..." << std::endl;
		for (int i = 0; i < total_chunks; i += 2) {
			deallocate(allocated[i], i);
		}
		std::cout << std::endl;

		std::cout << "deallocating odd..." << std::endl;
		for (int i = 1; i < total_chunks; i += 2) {
			deallocate(allocated[i], i);
		}
		std::cout << std::endl;

		std::cout << "allocating all..." << std::endl;
		for (int i = 0; i < total_chunks; i++) {
			allocate(allocated[i], i);
		}
		std::cout << std::endl;

		check_invalid_allocation();

		std::cout << "deallocating all.." << std::endl;
		for (int i = 0; i < total_chunks; i++) {
			deallocate(allocated[i], i);
		}
		std::cout << std::endl;

		std::cout << "final memory view: " << std::endl << mem_view_t{data, total_size, lines_per_block} << std::endl;

		std::cout << "Congratulations! We didn't crash! Yay!" << std::endl << std::endl;

		return 0;
	}

	int test_pool_wrapper() {
		if (test_pool_wrapper<1, 4>()) {
			return -1;
		}
		
		if (test_pool_wrapper<2, 4>()) {
			return -1;
		}
		
		if (test_pool_wrapper<3, 4>()) {
			return -1;
		}
		
		if (test_pool_wrapper<4, 4>()) {
			return -1;
		}
		
		if (test_pool_wrapper<5, 4>()) {
			return -1;
		}
		
		if (test_pool_wrapper<6, 4>()) {
			return -1;
		}
		
		if (test_pool_wrapper<7, 4>()) {
			return -1;
		}
		
		return 0;
	}
}

int main() {
	return test_pool_wrapper();
}