#include <iomanip>
#include <iostream>

#include <cuw/mem/page_alloc.hpp>
#include <cuw/mem/pool_alloc.hpp>

#include "utils.hpp"
#include "dummy_alloc.hpp"

using namespace cuw;

namespace {
	struct basic_alloc_traits_t {
		static constexpr bool use_resolved_page_size = true;
		static constexpr std::size_t alloc_page_size = block_size_t{1};
		static constexpr std::size_t alloc_block_pool_size = block_size_t{16};
		static constexpr std::size_t alloc_min_block_size = block_size_t{64};

		static constexpr std::size_t alloc_min_pool_power = mem::block_align_pow + 2; // 2^8
		static constexpr std::size_t alloc_max_pool_power = mem::block_align_pow + 4; // 2^10
		static constexpr std::size_t alloc_pool_cache_lookups = 4;
		static constexpr std::size_t alloc_raw_cache_lookups = 4;
		static constexpr std::size_t alloc_base_alignment = 16;
		static constexpr bool use_alloc_cache = false;
		static constexpr auto alloc_pool_first_chunk = mem::pool_chunk_size_t::Bytes2;
		static constexpr auto alloc_pool_last_chunk = mem::pool_chunk_size_t::Bytes128;
	};

	template<class alloc_t, class = void>
	struct is_page_alloc_t : std::false_type {};

	template<class alloc_t>
	struct is_page_alloc_t<alloc_t, std::void_t<decltype(&alloc_t::get_size_index), decltype(&alloc_t::get_addr_index)>> : std::false_type {};

	template<class alloc_t>
	inline constexpr bool is_page_alloc_v = is_page_alloc_t<alloc_t>::value;

	using pool_alloc_traits_t = mem::pool_alloc_traits_t<mem::page_alloc_traits_t<basic_alloc_traits_t>>;
	//using page_alloc_t = mem::page_alloc_t<dummy_allocator_t<pool_alloc_traits_t>>;
	// TODO : check dummy alloc for overlaps
	using page_alloc_t = dummy_allocator_t<pool_alloc_traits_t>;
	using pool_alloc_t = mem::pool_alloc_t<page_alloc_t>;

	inline constexpr std::size_t min_pool_chunk_size = mem::pool_chunk_size((std::size_t)pool_alloc_t::alloc_pool_first_chunk);
	inline constexpr std::size_t max_pool_chunk_size = mem::pool_chunk_size((std::size_t)pool_alloc_t::alloc_pool_last_chunk);
	inline constexpr std::size_t max_alignment = std::min(max_pool_chunk_size, pool_alloc_t::alloc_page_size);
	inline constexpr std::size_t max_alignment_power = std::countr_zero(max_alignment);

	int test_pool_alloc() {
		std::cout << "testing pool_alloc..." << std::endl;

		constexpr std::size_t page_size = pool_alloc_t::alloc_page_size;
		constexpr std::size_t basic_alloc_size = page_size << 10;

		pool_alloc_t alloc(basic_alloc_size, page_size);

		void* ptr{};

		// malloc(<any_size>)
		for (int size = 0; size <= 2 * max_pool_chunk_size; size = 2 * size + 1) {
			ptr = alloc.malloc(size);
			memset_deadbeef(ptr, size);
			if (!alloc.free(ptr)) {
				std::abort();
			}
		}

		// malloc(<any_size>, <any_alignment>)
		for (int size = 0; size <= 2 * max_pool_chunk_size; size = 2 * size + 1) {
			for (int alignment = 1; alignment <= max_alignment; alignment *= 2) {
				// standart API
				ptr = alloc.malloc(size, alignment);
				memset_deadbeef(ptr, size);
				if (!alloc.free(ptr)) {
					std::abort();
				}
				// extension API
				ptr = alloc.malloc(size, alignment);
				memset_deadbeef(ptr, size);
				if (!alloc.free(ptr, size, alignment)) {
					std::abort();
				}
			}
		}

		// realloc(nullptr, 0)
		ptr = alloc.realloc(nullptr, 0);
		if (!alloc.free(ptr)) {
			std::abort();
		}

		// realloc(<any_alloc>, <any_size>)
		for (int old_size = 0; old_size <= 2 * max_pool_chunk_size; old_size = 2 * old_size + 1) {
			for (int new_size = 0; new_size <= 2 * max_pool_chunk_size; new_size = 2 * new_size + 1) {
				ptr = alloc.malloc(old_size);
				memset_deadbeef(ptr, old_size);
				ptr = alloc.realloc(ptr, new_size);
				memset_deadbeef(ptr, new_size);
				if (!alloc.free(ptr)) {
					std::abort();
				}
			}
		}
		
		// realloc(nullptr, <any_size>, <any_size>, <any_alignment>, 0) -> malloc(<any_size>, <any_alignment>)
		for (int old_size = 0; old_size <= 2 * max_pool_chunk_size; old_size = 2 * old_size + 1) {
			for (int new_size = 0; new_size <= 2 * max_pool_chunk_size; new_size = 2 * new_size + 1) {
				for (int alignment = 1; alignment <= max_alignment; alignment *= 2) {
					ptr = alloc.realloc(nullptr, old_size, new_size, alignment);
					memset_deadbeef(ptr, new_size);
					if (!alloc.free(ptr, new_size, alignment)) {
						std::abort();
					}
				}
			}
		}

		// realloc(<any_alloc>, <any_old_size>, <any_new_size>, <any_alignment>)		
		for (int old_size = 0; old_size <= 2 * max_pool_chunk_size; old_size = 2 * old_size + 1) {
			for (int new_size = 0; new_size <= 2 * max_pool_chunk_size; new_size = 2 * new_size + 1) {
				for (int alignment = 1; alignment <= max_alignment; alignment *= 2) {
					ptr = alloc.malloc(old_size, alignment);
					memset_deadbeef(ptr, old_size);
					ptr = alloc.realloc(ptr, old_size, new_size, alignment);
					memset_deadbeef(ptr, new_size);
					if (!alloc.free(ptr, new_size, alignment)) {
						std::abort();
					}
				}
			}
		}

		// realloc(<any_alloc>, <any_old_size>, <any_new_size>, <any_alignment>), free using standart API
		for (int old_size = 0; old_size <= 2 * max_pool_chunk_size; old_size = 2 * old_size + 1) {
			for (int new_size = 0; new_size <= 2 * max_pool_chunk_size; new_size = 2 * new_size + 1) {
				for (int alignment = 1; alignment <= max_alignment; alignment *= 2) {
					ptr = alloc.malloc(old_size, alignment);
					memset_deadbeef(ptr, old_size);
					ptr = alloc.realloc(ptr, old_size, new_size, alignment);
					memset_deadbeef(ptr, new_size);
					if (!alloc.free(ptr)) {
						std::abort();
					}
				}
			}
		}

		// free(nullptr)
		if (!alloc.free(nullptr)) {
			std::abort();
		}

		std::cout << "testing finished" << std::endl;
		return 0;
	}

	int test_pool_growth_impl(pool_alloc_t& alloc, std::size_t size, std::size_t alignment) {
		std::cout << "testing pool growth for size " << size << "..." << std::endl;

		// size must be pow of 2
		if (!mem::is_alignment(size) || !mem::is_alignment(alignment)) {
			std::cerr << "size of alignment is not power of 2" << std::endl;
			std::abort();
		}

		auto geom_sum2 = [] (std::size_t a, std::size_t b) {
			assert(b >= a);
			std::size_t m = ~(std::size_t)0;
			return ~(m << (b - a + 1)) << a;
		};

		std::size_t size_aligned = mem::align_value(size, alignment);
		std::size_t count = geom_sum2(pool_alloc_t::alloc_min_pool_power, pool_alloc_t::alloc_max_pool_power) / size_aligned;
		std::vector<void*> allocations(count);

		std::vector<char> deadbeef(size);
		memset_deadbeef(deadbeef.data(), size);

		auto allocate = [&] (int start, int end, int step) {
			for (int i = start; i < end; i += step) {
				allocations[i] = alloc.malloc(size, alignment);
				memset_deadbeef(allocations[i], size);
			}	
		};

		auto deallocate = [&] (int start, int end, int step) {
			for (int i = start; i < end; i += step) {
				if (std::memcmp(allocations[i], deadbeef.data(), size) != 0) {
					std::cerr << "invalid check value" << std::endl;
					std::abort();
				}
				if (!alloc.free(allocations[i], size, alignment)) {
					std::abort();
				}
			}
		};

		std::cout << "allocating all..." << std::endl;
		allocate(0, count, 1);
		std::cout << "deallocating all..." << std::endl;
		deallocate(0, count, 1);

		std::cout << "allocating all..." << std::endl;
		allocate(0, count, 1);
		std::cout << "deallocating even.." << std::endl;
		deallocate(0, count, 2);
		std::cout << "deallocating odd..." << std::endl;
		deallocate(1, count, 2);

		std::cout << "allocating even..." << std::endl;
		allocate(0, count, 2);
		std::cout << "allocating odd..." << std::endl;
		allocate(1, count, 2);
		std::cout << "deallocating first half..." << std::endl;
		deallocate(0, count / 2, 1);
		std::cout << "deallocating second half..." << std::endl;
		deallocate(count / 2, count, 1);

		std::cout << "allocating all..." << std::endl;
		allocate(0, count, 1);
		std::cout << "deallocating all..." << std::endl;
		deallocate(0, count, 1);

		std::cout << "testing finished" << std::endl;
		return 0;
	}

	int test_pool_growth() {
		std::cout << "testing pool growth..." << std::endl;

		constexpr std::size_t page_size = pool_alloc_t::alloc_page_size;
		constexpr std::size_t basic_alloc_size = page_size << 10;

		pool_alloc_t alloc(basic_alloc_size, page_size);
		for (int size = 1; size <= max_pool_chunk_size; size *= 2) {
			for (int alignment = 1; alignment <= max_alignment; alignment *= 2) {
				test_pool_growth_impl(alloc, size, alignment);
				std::cout << std::endl;
			}
		}
		
		std::cout << "testing finished" << std::endl;

		return 0;
	}

	struct allocation_t {
		void* ptr{};
		std::size_t size{};
		std::size_t alignment{};
	};

	std::ostream& operator << (std::ostream& os, const allocation_t& alloc) {
		return os << "[" << pretty(alloc.ptr) << ":" << alloc.size << ":" << alloc.alignment << "]";
	}

	int test_pool_alloc_random() {
		std::cout << "testing by random allocations..." << std::endl;

		constexpr std::size_t page_size = pool_alloc_t::alloc_page_size;
		constexpr std::size_t basic_alloc_size = page_size << 12;
		constexpr std::size_t cmd_count = 10; // 8 is a good test case
		constexpr std::size_t max_req_alloc_size = 1 << 10;
		constexpr std::size_t max_req_alignment = max_alignment_power;

		int_gen_t gen(42);
		pool_alloc_t alloc(basic_alloc_size, page_size);
		std::vector<allocation_t> allocations;

		auto print_status = [&] () {
			if constexpr(is_page_alloc_v<pool_alloc_t>) {
				std::cout << "- addr_index" << std::endl << addr_index_info_t{alloc};
				std::cout << "- size_index" << std::endl << size_index_info_t{alloc};
			} std::cout << "- alloc ranges" << std::endl << ranges_info_t{alloc};
		};

		auto allocate = [&] () {
			std::size_t size = gen.gen(max_req_alloc_size + 1);
			std::size_t alignment = 1 << gen.gen(max_req_alignment + 1);
			std::cout << "allocating " << size << " bytes aligned by " << alignment << "..." << std::endl;
			if (void* ptr = alloc.malloc(size, alignment)) {
				allocations.push_back({ptr, size, alignment});
				std::cout << "successully allocated memory " << allocations.back() << std::endl;
			} else {
				std::cout << "failed to allocate memory" << std::endl;
			} print_status();
		};

		auto deallocate = [&] () {
			if (!allocations.empty()) {
				int index = gen.gen(allocations.size());
				auto& allocation = allocations[index];
				std::cout << "deallocating " << allocation << std::endl;
				if (!alloc.free(allocation.ptr, allocation.size, allocation.alignment)) {
					std::abort();
				}
				allocations[index] = allocations.back();
				allocations.pop_back();
				std::cout << "successfully deallocated memory" << std::endl;
				print_status();
			} else {
				std::cout << "no allocations" << std::endl;
			}
		};

		auto reallocate = [&] () {
			if (!allocations.empty()) {
				int index = gen.gen(allocations.size());
				std::size_t new_size = gen.gen(max_req_alloc_size + 1);
				auto& allocation = allocations[index];
				std::cout << "reallocating memory " << allocation << " to size " << new_size << std::endl;
				if (void* ptr = alloc.realloc(allocation.ptr, allocation.size, new_size, allocation.alignment)) {
					allocation.ptr = ptr;
					allocation.size = new_size;
					std::cout << "successfully reallocated memory to " << allocation << std::endl;
				} else {
					std::cout << "failed to reallocate memory" << std::endl;
				} print_status();
			} else {
				std::cout << "no allocations" << std::endl;
			}
		};

		for (int i = 0; i < cmd_count; i++) {
			std::cout << "cmd: " << i << std::endl;
			int cmd = gen.gen(3);
			switch (cmd) {
				case 0: {
					allocate();
					break;
				} case 1: {
					deallocate();
					break;
				} case 2: {
					reallocate();
					break;
				}
			} std::cout << std::endl;
		}

		for (auto& [ptr, size, align] : allocations) {
			if (!alloc.free(ptr, size, align)) {
				std::abort();
			}
		}

		std::cout << "testing finished" << std::endl;

		return 0;
	}	
}

int main() {
	if (test_pool_alloc()) {
		return -1;
	} std::cout << std::endl;
	
	if (test_pool_growth()) {
		return -1;
	} std::cout << std::endl;
	
	if (test_pool_alloc_random()) {
		return -1;
	} std::cout << std::endl;

	return 0;
}