#include "cuwalot.hpp"

#include "config.hpp"
#include "sys_alloc.hpp"
#include "page_alloc.hpp"
#include "pool_alloc.hpp"
#include "cached_alloc.hpp"

#include <mutex>

namespace cuw::mem {
	using basic_allocator_t = pool_alloc_t<page_alloc_t<sys_alloc_t<config_traits_t>>>;

	class allocator_t {
	public:
		static allocator_t& get() {
			static allocator_t allocator;
			return allocator;
		}

		// standart API
		void* malloc(std::size_t size) {
			std::unique_lock lock_guard{lock};
			return allocator.malloc(size);
		}

		void* realloc(void* ptr, std::size_t new_size) {
			std::unique_lock lock_guard{lock};
			return allocator.realloc(ptr, new_size);
		}

		void free(void* ptr) {
			std::unique_lock lock_guard{lock};
			if (!allocator.free(ptr)) {
				std::abort();
			}
		}

		// extension API
		void* malloc(std::size_t size, std::size_t alignment, flags_t flags) {
			std::unique_lock lock_guard{lock};
			return allocator.malloc(size, alignment, flags);
		}

		void* realloc(void* ptr, std::size_t old_size, std::size_t new_size, std::size_t alignment, flags_t flags) {
			std::unique_lock lock_guard{lock};
			return allocator.realloc(ptr, old_size, new_size, alignment, flags);
		}

		void free(void* ptr, std::size_t size, std::size_t alignment, flags_t flags) {
			std::unique_lock lock_guard{lock};
			if (!allocator.free(ptr, size, alignment, flags)) {
				std::abort();
			}
		}

	private:
		std::mutex lock{};
		basic_allocator_t allocator{};
	};


	// standart API
	void* malloc(std::size_t size) {
		return allocator_t::get().malloc(size);
	}

	void* realloc(void* ptr, std::size_t new_size) {
		return allocator_t::get().realloc(ptr, new_size);
	}

	void free(void* ptr) {
		allocator_t::get().free(ptr);
	}

	// extension API, full spec
	void* malloc_ext(std::size_t size, std::size_t alignment, flags_t flags) {
		return allocator_t::get().malloc(size, alignment, flags);
	}

	void* realloc_ext(void* ptr, std::size_t old_size, std::size_t new_size, std::size_t alignment, flags_t flags) {
		return allocator_t::get().realloc(ptr, old_size, new_size, alignment, flags);
	}

	void free_ext(void* ptr, std::size_t size, std::size_t alignment, flags_t flags) {
		return allocator_t::get().free(ptr, size, alignment, flags);
	}
}