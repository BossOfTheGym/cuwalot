#include <random>
#include <vector>
#include <chrono>

#include <cuw/mem/cuwalot.hpp>

#include "utils.hpp"

using ticks_t = std::int64_t;

ticks_t get_current_time() {
	return std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::high_resolution_clock::now().time_since_epoch()
	).count();
}

const char* tick_label = "us";

template<class malloc_func_t, class free_func_t>
void test_random_stuff(bool deadbeef, malloc_func_t&& malloc_func, free_func_t&& free_func) {
	constexpr int allocation_count = 1000000;
	constexpr int allocation_size_min = (1 << 12) - 1;
	constexpr int allocation_size_max = (1 << 12);

	int_gen_t cmd_gen{420};
	int_gen_t size_gen{69};

	struct allocation_t {
		void* ptr{};
		std::size_t size{};
	};

	std::vector<allocation_t> allocations;
	allocations.reserve(allocation_count);

	ticks_t t0 = get_current_time();
	for (int i = 0; i < allocation_count; i++) {
		std::size_t size = size_gen.gen(allocation_size_min, allocation_size_max);
		void* ptr = malloc_func(size);
		allocations.push_back({ptr, size});
		if (deadbeef) {
			memset_deadbeef(ptr, size);
		}
	}

	for (auto [ptr, size] : allocations) {
		free_func(ptr, size);
	}
	ticks_t t1 = get_current_time();

	std::cout << "elapsed: " << t1 - t0 << tick_label << std::endl;
}

void test_std_alloc(int k) {
	auto std_malloc_func = [](std::size_t size) {
		return std::malloc(size);
	};

	auto std_free_func = [](void* ptr, std::size_t size) {
		std::free(ptr);
	};

	std::cout << "*** testing stdalloc ***" << std::endl;
	for (int i = 0; i < k; i++) {
		test_random_stuff(true, std_malloc_func, std_free_func);
	}

	std::cout << std::endl;
}

void test_cuw_alloc(int k) {
	auto cuw_malloc_func = [](std::size_t size) {
		return cuw::mem::malloc_ext(size);
	};
	
	auto cuw_free_func = [](void* ptr, std::size_t size) {
		cuw::mem::free_ext(ptr, size);
	};

	std::cout << "*** testing cuwalloc ***" << std::endl;
	for (int i = 0; i < k; i++) {
		test_random_stuff(true, cuw_malloc_func, cuw_free_func);
	}
	std::cout << std::endl;
}

int main() {
	test_std_alloc(7);
	test_cuw_alloc(7);
 	return 0;
}