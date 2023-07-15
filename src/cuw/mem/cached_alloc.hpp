#pragma once

#include "core.hpp"
#include "alloc_tag.hpp"

namespace cuw::mem {
	enum class cached_alloc_flags_t : flags_t {
		Exact,
		Any,
	};

	template<class basic_alloc_t>
	class cached_alloc_t : public basic_alloc_t {
	public:
		using base_t = basic_alloc_t;
		static_assert(has_sysmem_alloc_tag_v<base_t>);

		template<class ... args_t>
		cached_alloc_t(args_t&& ... args) : base_t(std::forward<args_t>(args)...) {}

		cached_alloc_t() = default;
		cached_alloc_t(cached_alloc_t&&) noexcept = delete;
		cached_alloc_t(const cached_alloc_t&) = delete;

		~cached_alloc_t() {
			flush_slots();
		}

		cached_alloc_t& operator = (cached_alloc_t&&) noexcept = delete;
		cached_alloc_t& operator = (const cached_alloc_t&) = delete;

	private:
		static constexpr std::size_t slot_count = base_t::alloc_cache_slots;
		static constexpr std::size_t min_slot_size = base_t::alloc_min_slot_size;
		static constexpr std::size_t max_slot_size = base_t::alloc_max_slot_size;

		struct slot_t {
			void* allocate(std::size_t alloc_size) {
				if (size < alloc_size) {
					return nullptr;
				}
				void* allocated = ptr;
				if (alloc_size != size) {
					ptr = (char*)ptr + alloc_size;
					size -= alloc_size;
				} else {
					ptr = nullptr;
					size = 0;
				} return allocated;
			}

			void reset() {
				ptr = nullptr;
				size = 0;
			}

			bool is_null() const {
				return ptr == nullptr;
			}

			void* get_ptr() const {
				return ptr;
			}

			std::size_t get_size() const {
				return size;
			}

			std::tuple<void*, std::size_t> to_tuple() const {
				return std::make_tuple(ptr, size);
			}

			// if ptr is 0 then size is zero
			void* ptr{};
			std::size_t size{};
		};

		// find appropriate slot and allocate from it
		void* allocate_from_slots(std::size_t size) {
			assert(size != 0);
			for (auto& slot : slots) {
				void* ptr = slot.allocate(size);
				if (!ptr) {
					continue;
				}

				if (!slot.is_null() && slot.get_size() < min_slot_size) {
					base_t::deallocate(slot.get_ptr(), slot.get_size());
					slot.reset();
				}

				return ptr;
			}

			return nullptr;
		}

		// find slot of maximum capacity and allocate whole memory from it, can be empty
		std::tuple<void*, std::size_t> allocate_max() {
			auto& slot = *std::max_element(std::begin(slots), std::end(slots), [&] (auto& s0, auto& s1) {
				return s0.get_size() < s1.get_size();
			});

			auto alloc = slot.to_tuple();
			slot.reset();
			return alloc;
		}

		void fill_slots(void* ptr, std::size_t size) {
			assert(ptr);
			assert(size != 0);

			std::sort(std::begin(slots), std::end(slots), [&] (auto& a, auto& b) {
				return a.get_size() < b.get_size();
			});

			slot_t chunk{ptr, size};
			for (auto& slot : slots) {
				std::size_t slot_size = std::min(chunk.get_size(), max_slot_size);
				if (slot_size < min_slot_size) {
					break;
				}
				
				if (slot.get_size() < slot_size) {
					if (!slot.is_null()) {
						base_t::deallocate(slot.get_ptr(), slot.get_size());
					} slot = slot_t{chunk.allocate(slot_size), slot_size};
				}
				
				if (chunk.is_null()) {
					break;
				}
			}
			
			if (!chunk.is_null()) {
				base_t::deallocate(chunk.get_ptr(), chunk.get_size());
			}
		}

	public:
		void* allocate(std::size_t size) {
			size = align_value(size, base_t::get_page_size());
			if (void* ptr = allocate_from_slots(size)) {
				return ptr;
			}	
			return base_t::allocate(size);
		}

		void deallocate(void* ptr, std::size_t size) {
			fill_slots(ptr, size);
		}

		void* reallocate(void* old_ptr, std::size_t old_size, std::size_t new_size) {
			if (void* new_ptr = allocate_from_slots(new_size)) {
				std::memcpy(new_ptr, old_ptr, old_size);
				deallocate(old_ptr, old_size);
				return new_ptr;
			}
			return base_t::reallocate(old_ptr, old_size, new_size);
		}

	public:
		std::tuple<void*, std::size_t> allocate_ext(std::size_t size, cached_alloc_flags_t flags) {
			switch (flags) {
				case cached_alloc_flags_t::Exact: {
					return {allocate(size), size};
				}
				
				case cached_alloc_flags_t::Any: {
					// TODO : we scan slots here twice, maybe it'd be better if we scan them once?
					if (void* ptr = allocate_from_slots(size); ptr) {
						return {ptr, size};
					} if (auto [ptr, true_size] = allocate_max(); ptr) {
						return {ptr, true_size};
					} return base_t::allocate(size);
				}
				
				default: {
					std::abort();
				}
			}
		}

		void flush_slots() {
			for (auto& slot : slots) {
				if (!slot.is_null()) {
					base_t::deallocate(slot.get_ptr(), slot.get_size());
					slot.reset();
				}
			}
		}

	private:
		slot_t slots[slot_count] = {};
	};
}