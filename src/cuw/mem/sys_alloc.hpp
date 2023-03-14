#pragma once

#include "core.hpp"
#include "mem_api.hpp"
#include "alloc_tag.hpp"

namespace cuw::mem {
	template<class __traits_t>
	class sys_alloc_t : public __traits_t {
	public:
		using tag_t = sysmem_alloc_tag_t;
		using traits_t = __traits_t;

		sys_alloc_t() = default;
		sys_alloc_t(sys_alloc_t&&) noexcept = delete;
		sys_alloc_t(const sys_alloc_t&) = delete; 

		sys_alloc_t& operator = (sys_alloc_t&&) noexcept = delete;
		sys_alloc_t& operator = (const sys_alloc_t&) = delete; 

		void adopt(sys_alloc_t&) {}

		[[nodiscard]] void* allocate(std::size_t size) {
			assert(size != 0);
			auto [ptr, _] = allocate_sysmem(size);
			return ptr;
		}

		void deallocate(void* ptr, std::size_t size) {
			assert(size != 0);
			deallocate_sysmem(ptr, size);
		}

		[[nodiscard]] void* reallocate(void* old_ptr, std::size_t old_size, std::size_t new_size) {
			assert(old_size != 0);
			assert(new_size != 0);
			void* new_ptr = allocate(new_size);
			if (new_ptr) {
				memcpy(new_ptr, old_ptr, std::min(old_size, new_size));
				deallocate(old_ptr, old_size);
				return new_ptr;
			} return nullptr;
		}
	};
}