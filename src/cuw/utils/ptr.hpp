#pragma once

#include <new>
#include <cstddef>
#include <cassert>

namespace cuw {
	template<class dst_type_t, class src_type_t>
	dst_type_t* transform_ptr(src_type_t* ptr, std::ptrdiff_t diff) {
		return std::launder((dst_type_t*)((char*)(ptr) + diff));
	}

	template<class obj_t, class base_t>
	obj_t* __base_to_obj(base_t* base, std::ptrdiff_t offset) {
		assert(base);
		return std::launder((obj_t*)((char*)base - offset));
	}

	#define base_to_obj(base, obj, field) __base_to_obj<obj>((base), offsetof(obj, field))

	// little tagged_ptr utility
	// you can store and retrieve a pointer from here as well as data
	// no pointer arithmetic is implemented as... it doesn't make any sense in this context
	template<class __type_t, size_t __bits>
	struct alignas(__type_t*) tagged_ptr_t {
		using type_t = __type_t;

		static constexpr std::uintptr_t bits = __bits;
		static constexpr std::uintptr_t ptr_mask = (~(size_t)0) << bits;
		static constexpr std::uintptr_t data_mask = ~ptr_mask;

		static_assert(sizeof(std::uintptr_t) == sizeof(type_t*) && alignof(std::uintptr_t) == alignof(type_t*),
			"size or alignment of std::uintptr_t are not the same as of type_t*");

		tagged_ptr_t(type_t* ptr = nullptr) {
			set_ptr(ptr);
		}

		tagged_ptr_t(type_t* ptr, std::uintptr_t data) {
			set_ptr_data(ptr, data);
		}

		tagged_ptr_t(const tagged_ptr_t&) = default;
		tagged_ptr_t(tagged_ptr_t&&) = default;

		tagged_ptr_t& operator = (const tagged_ptr_t&) = default;
		tagged_ptr_t& operator = (tagged_ptr_t&&) = default;

		tagged_ptr_t& operator = (type_t* ptr) {
			set_ptr(ptr);
			return *this;
		}

		type_t* operator -> () {
			return get_ptr();
		}

		type_t& operator * () {
			return *get_ptr();
		}

		type_t const* operator -> () const {
			return get_ptr();
		}

		const type_t& operator * () const {
			return *get_ptr();
		}

		template<class ptr_type_t>
		operator ptr_type_t* () const {
			return (ptr_type_t*)get_ptr();
		}

		template<class int_type_t>
		explicit operator int_type_t () const {
			return (int_type_t)get_ptr();
		}

		// we'll make assumption that data does not contain inappropriate bits (pointer can contain inappropriate bits)
		// so it's your fault if something goes completely wrong

		// ptr must not contain inappropriate bits (tagged_ptr cast to pointer returns good pointer)
		void set_ptr(type_t* ptr) {
			ptr_data = (std::uintptr_t)ptr | (ptr_data & data_mask);
		}

		// data must not contain inappropriate bits
		void set_data(std::uintptr_t data) {
			set_ptr_data((type_t*)ptr_data, data);
		}

		// data must not contain inappropriate bits, ptr can
		void set_ptr_data(type_t* ptr, std::uint64_t data) {
			assert((data & ptr_mask) == 0);
			ptr_data = ((std::uintptr_t)ptr & ptr_mask) | (std::uintptr_t)data;
		}

		type_t* get_ptr() const {
			return (type_t*)((std::uintptr_t)ptr_data & ptr_mask);
		}

		std::uintptr_t get_data() const {
			return (std::uintptr_t)ptr_data & data_mask; 
		}

		std::uintptr_t ptr_data{};
	};
}