#include <vector>
#include <iostream>

#include <cuw/mem/core.hpp>
#include <cuw/mem/alloc_descr.hpp>
#include <cuw/mem/alloc_entries.hpp>
#include <cuw/mem/alloc_wrappers.hpp>

using namespace cuw;

namespace {
	using ad_t = mem::alloc_descr_t;
	using attrs_t = mem::attrs_t;

	class test_ad_t {
	public:
		test_ad_t(attrs_t type, attrs_t chunk_enum, std::size_t chunk_size, std::size_t size, void* data) {
			std::size_t capacity = size / chunk_size;
			descr = ad_t {
				.size = size,
				.type = type, .chunk_size = chunk_enum, .capacity = capacity, .head = mem::alloc_descr_head_empty,
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

	test_ad_t create_byte_pool(std::size_t size, void* data) {
		return test_ad_t((attrs_t)mem::block_type_t::PoolBytes, mem::type_to_pool_chunk<mem::byte_pool_t, attrs_t>(), sizeof(mem::byte_pool_t), size, data);
	}

	int test_byte_pool_wrapper() {
		constexpr std::size_t total_chunks = 4;
		constexpr std::size_t total_size = total_chunks * sizeof(mem::byte_pool_t); 
		constexpr std::size_t allocs_per_pool = mem::byte_pool_t::max_chunks;
		constexpr std::size_t total_allocs = total_chunks * allocs_per_pool;

		alignas(sizeof(mem::byte_pool_t)) std::uint8_t chunks[total_size] = {};

		std::uint8_t* allocated[total_allocs] = {};

		std::size_t size = total_size;
		test_ad_t test_ad = create_byte_pool(size, chunks);
		mem::byte_pool_wrapper_t wrapper(test_ad);

		auto allocate = [&] (auto& chunk, auto check) {
			chunk = (std::uint8_t*)wrapper.acquire_chunk();
			if (!chunk) {
				std::cerr << "something went wrong..." << std::endl;
				std::abort();
			}
			std::cout << "allocated: " << (void*)chunk << std::endl;
			*chunk = check;
		};

		auto deallocate = [&] (auto& chunk, auto check) {
			if (*chunk != check) {
				std::cerr << "invalid check value" << std::endl;
				std::abort();
			}
			std::cout << "deallocating: " << (void*)chunk << std::endl;
			wrapper.release_chunk(chunk);
			chunk = nullptr;
		};

		std::cout << "allocating all..." << std::endl;
		for (int i = 0; i < total_allocs; i++) {
			allocate(allocated[i], i);
		}
		
		std::cout << "deallocating all..." << std::endl; 
		for (int i = 0; i < total_allocs; i++) {
			deallocate(allocated[i], i);
		}

		std::cout << "allocating all..." << std::endl;
		for (int i = 0; i < total_allocs; i++) {
			allocate(allocated[i], i);
		}

		std::cout << "deallocating even chunks..." << std::endl;
		for (int i = 0; i < total_allocs; i += 2) {
			deallocate(allocated[i], i);
		}

		std::cout << "deallocating odd chunks..." << std::endl;
		for (int i = 1; i < total_allocs; i += 2) {
			deallocate(allocated[i], i);
		}

		std::cout << "allocating all..." << std::endl;
		for (int i = 0; i < total_allocs; i++) {
			allocate(allocated[i], i);
		}

		std::cout << "deallocating even pools..." << std::endl;
		for (int k = 0; k < total_chunks; k += 2) {
			// TODO
		}

		std::cout << "deallocating odd pools..." << std::endl;
		for (int k = 0; k < total_chunks; k += 2) {
			// TODO
		}

		return 0;
	}

	test_ad_t create_pool(mem::pool_chunk_size_t chunk_size, std::size_t size, void* data) {
		return test_ad_t((attrs_t)mem::block_type_t::Pool, (attrs_t)chunk_size, mem::pool_chunk_size((attrs_t)chunk_size), size, data);
	}

	int test_pool_wrapper() {
		return 0;
	}
}

int main() {
	if (test_byte_pool_wrapper()) {
		return -1;
	} if (test_pool_wrapper()) {
		return -1;
	} return 0;
}