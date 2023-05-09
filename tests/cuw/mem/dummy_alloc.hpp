#pragma once

#include <set>
#include <vector>
#include <iomanip>
#include <iostream>

#include <cuw/mem/alloc_tag.hpp>

#include "utils.hpp"

using namespace cuw;

namespace {
	std::tuple<void*, void*> alloc_aligned(std::size_t size, std::size_t alignment) {
		void* base_ptr = std::malloc(size + alignment);
		void* ptr = (void*)mem::align_value<std::uintptr_t>((std::uintptr_t)base_ptr, alignment);
		return {base_ptr, ptr};
	} 

	struct range_t {
		static range_t create(std::size_t size, std::size_t alignment) {
			auto [base_ptr, ptr] = alloc_aligned(size, alignment);
			return {base_ptr, ptr, size};
		}

		bool operator < (void* another) const {
			return (std::uintptr_t)ptr < (std::uintptr_t)another;
		}

		bool operator < (const range_t& another) const {
			return (std::uintptr_t)ptr < (std::uintptr_t)another.ptr;
		}

		void* get_start() const {
			return ptr;
		}

		void* get_end() const {
			return (char*)ptr + size;
		}

		bool has_ptr(void* checked) const {
			return (std::uintptr_t)get_start() <= (std::uintptr_t)checked
				&& (std::uintptr_t)checked < (std::uintptr_t)get_end();
		}

		bool overlaps(const range_t& another) const {
			auto l1 = (std::uintptr_t)get_start();
			auto r1 = (std::uintptr_t)get_start();
			auto l2 = (std::uintptr_t)another.get_start();
			auto r2 = (std::uintptr_t)another.get_start();
			return l2 < r1 && l1 < r2;
		}

		void* base_ptr{};
		void* ptr{};
		std::size_t size{};
	};

	std::ostream& operator << (std::ostream& os, const range_t& range) {
		os << "[" << range.get_start() << "-" << range.get_end() << ":" << range.size << "]";
		return os;
	}

	using range_set_t = std::set<range_t>;

	template<class __traits_t>
	class dummy_allocator_t : public __traits_t {
	public:
		using tag_t = mem::sysmem_alloc_tag_t;
		using traits_t = __traits_t;

		dummy_allocator_t(std::size_t size, std::size_t alignment) : page_size{alignment} {
			auto range = range_t::create(size, alignment);
			alloc_ranges.insert(range);
			ranges.insert(range);
			std::cout << "initial alloc range: " << range << std::endl;
		}

		dummy_allocator_t(std::initializer_list<range_t> _ranges, std::size_t alignment) : page_size{alignment} {
			alloc_ranges.insert(_ranges.begin(), _ranges.end());
			for (auto& range : _ranges) {
				assert(mem::is_aligned(range.ptr, alignment));
				return_range(range);
			} for (auto& range : alloc_ranges) {
				std::cout << "initial alloc range: " << range << std::endl;
			}
		}

		dummy_allocator_t(dummy_allocator_t&&) noexcept = delete;
		dummy_allocator_t(const dummy_allocator_t&) = delete;

		~dummy_allocator_t() {
			for (auto& [base, ptr, size] : alloc_ranges) {
				free(base);
			}
		}

		dummy_allocator_t& operator = (dummy_allocator_t&&) noexcept = delete;
		dummy_allocator_t& operator = (const dummy_allocator_t&) = delete;

		void adopt(dummy_allocator_t& another) {
			if (this == &another) {
				return;
			}

			std::cout << "adoptiong another allocator" << std::endl;
			alloc_ranges.insert(another.alloc_ranges.begin(), another.alloc_ranges.end());
			another.alloc_ranges.clear();
			for (auto& range : another.ranges) {
				return_range(range);
			} another.ranges.clear();
		}

	private:
		void return_range(const range_t& range) {
			auto [it, inserted] = ranges.insert(range);
			assert(inserted);

			auto merge_prev_curr = [&] (auto prev, auto curr) {
				if (curr == ranges.end()) {
					std::cerr << "curr is invalid" << std::endl;
					std::abort();
				} if (prev != ranges.end()) {
					std::cout << "merging " << *prev << " and " << *curr << std::endl; 
					if (prev->overlaps(*curr)) {
						std::cerr << "overlap detected" << std::endl;
						std::abort();
					} if (prev->get_end() == curr->get_start()) {
						std::cout << "merging two ranges: " << *prev << " " << *curr << std::endl;
						range_t new_range{nullptr, prev->ptr, prev->size + curr->size};
						ranges.erase(prev);
						ranges.erase(curr);
						auto [new_curr, new_inserted] = ranges.insert(new_range);
						assert(new_inserted);
						std::cout << "merged into: " << *new_curr << std::endl;
						return new_curr;
					}
				} return curr;
			};

			auto merge_curr_next = [&] (auto curr, auto next) {
				if (curr == ranges.end()) {
					std::cerr << "curr is invalid" << std::endl;
					std::abort();
				} if (next != ranges.end()) {
					std::cout << "merging " << *curr << " and " << *next << std::endl; 
					if (curr->overlaps(*next)) {
						std::cerr << "overlap detected" << std::endl;
						std::abort();
					} if (curr->get_end() == next->get_start()) {
						std::cout << "merging two ranges: " << *curr << " " << *next << std::endl;
						range_t new_range{nullptr, curr->ptr, curr->size + next->size};
						ranges.erase(curr);
						ranges.erase(next);
						auto [new_curr, new_inserted] = ranges.insert(new_range);
						assert(new_inserted);
						std::cout << "merged into: " << *new_curr << std::endl;
						return new_curr;
					}
				} return curr;
			};

			auto prev = it; --prev;
			auto curr = it;
			auto next = it; ++next;
			merge_curr_next(merge_prev_curr(prev, curr), next);
			std::cout << std::endl;
		}

	public:
		[[nodiscard]] void* allocate(std::size_t size) {
			size = mem::align_value(size, page_size);

			std::cout << "making allocation of size " << size << std::endl;
			auto i = ranges.begin(), e = ranges.end();
			while (i != e) {
				auto range = *i;
				if (range.size >= size) {
					std::cout << "found block: " << range << std::endl;
					void* ptr = range.ptr;
					range.ptr = (char*)range.ptr + size;
					range.size -= size;
					ranges.erase(i);
					if (range.size > 0) {
						ranges.insert(range);
					}
					std::cout << "allocated memory: " << ptr << std::endl << std::endl;
					return ptr;
				} ++i;
			}
			std::cout << "failed to make allocation of size " << size << std::endl << std::endl;
			return nullptr;
		}

		void deallocate(void* ptr, std::size_t size) {
			size = mem::align_value(size, page_size);

			range_t range{ptr, ptr, size};
			std::cout << "deallocating range: " << range << std::endl;
			return_range(range);
		}

		[[nodiscard]] void* reallocate(void* old_ptr, std::size_t old_size, std::size_t new_size) {
			old_size = mem::align_value(old_size, page_size);
			new_size = mem::align_value(new_size, page_size);

			std::cout << "reallocating memory " << old_ptr << " of size " << old_size << " to new size " << new_size << std::endl;
			if (void* new_ptr = allocate(new_size)) {
				std::memcpy(new_ptr, old_ptr, std::min(old_size, new_size));
				deallocate(old_ptr, old_size);
				std::cout << "memory successfully reallocated: " << new_ptr << std::endl << std::endl;
				return new_ptr;
			}
			std::cout << "failed to reallocate memory" << std::endl << std::endl;
			return nullptr;
		}

	public:
		[[nodiscard]] void* allocate_hint(void* hint, std::size_t size) {
			hint = mem::align_value(hint, page_size);
			size = mem::align_value(size, page_size);

			std::cout << "making allocation of size " << size << " using hint " << hint << std::endl;

			auto it = ([&] () {
				auto i = ranges.begin(), e = ranges.end();
				while (i != e) {
					if (i->has_ptr(hint)) {
						return i;
					} ++i;
				} return e;
			})();
			if (it == ranges.end()) {
				std::cout << "error: range was not found" << std::endl;
				return nullptr;
			}

			auto a = (std::uintptr_t)it->get_start();
			auto b = (std::uintptr_t)it->get_end();
			auto c = (std::uintptr_t)hint;
			if (c < a || c + size > b) {
				std::cout << "found range " << *it << " is invalid or too small" << std::endl;
				return nullptr;
			}

			ranges.erase(it);
			if (c != a) {
				auto new_range = range_t{nullptr, (void*)a, c - a};
				std::cout << "inserting new range " << new_range << std::endl;
				ranges.insert(new_range);
			} if (c + size != b) {
				auto new_range = range_t{nullptr, (void*)(c + size), b - c - size};
				std::cout << "inserting new range " << new_range << std::endl;
				ranges.insert(new_range);
			}
			std::cout << "allocated memory: " << hint << std::endl << std::endl;
			return hint;
		}

		[[nodiscard]] void* reallocate_hint(void* hint, void* old_ptr, std::size_t old_size, std::size_t new_size) {
			std::cout << "reallocating memory " << old_ptr << " of size " << old_size << " to new size" << new_size << " using hint " << hint << std::endl;
			if (void* new_ptr = allocate_hint(hint, new_size)) {
				std::memcpy(new_ptr, old_ptr, std::min(old_size, new_size));
				deallocate(old_ptr, old_size);
				std::cout << "memory successfully reallocated: " << new_ptr << std::endl << std::endl;
				return new_ptr;
			}
			std::cout << "failed to reallocate memory" << std::endl << std::endl;
			return nullptr;
		}

	public:
		auto begin() {
			return ranges.begin();
		}

		auto end() {
			return ranges.end();
		}

		auto begin() const {
			return ranges.begin();
		}

		auto end() const {
			return ranges.end();
		}

		const range_set_t& get_alloc_ranges() const {
			return alloc_ranges;
		}

		const range_set_t& get_ranges() const {
			return ranges;
		}

		std::size_t get_page_size() const {
			return page_size;
		}

	private:
		range_set_t alloc_ranges{};
		range_set_t ranges;
		std::size_t page_size{};
	};

	struct alloc_request_t {
		static alloc_request_t allocate(std::size_t size, void* hint) {
			return alloc_request_t{nullptr, 0, size, hint};
		}

		static alloc_request_t reallocate(void* ptr, std::size_t old_size, std::size_t new_size, void* hint) {
			return alloc_request_t{ptr, old_size, new_size, hint};
		}

		bool is_alloc_request() const {
			return ptr == nullptr && requested_old_size == 0;
		}

		bool matches_alloc_params(std::size_t size) {
			return requested_new_size == size;
		}

		bool is_realloc_request() const {
			return !is_alloc_request();
		}

		bool matches_realloc_params(void* _ptr, std::size_t old_size, std::size_t new_size) const {
			return ptr == _ptr && old_size == requested_old_size && new_size == requested_new_size;
		}

		void* ptr{};
		std::size_t requested_old_size{};
		std::size_t requested_new_size{};
		void* hint{};
	};

	using request_stack_t = std::vector<alloc_request_t>;

	template<class type_t>
	using get_traits_t = typename type_t::traits_t;

	template<class proxy_t>
	class dummy_alloc_proxy_t : public get_traits_t<proxy_t> {
	public:
		using tag_t = typename proxy_t::tag_t;
		using traits_t = typename proxy_t::traits_t;

		dummy_alloc_proxy_t(proxy_t& _alloc) : alloc(_alloc) {}

		void adopt(dummy_alloc_proxy_t& another) {
			alloc.adopt(another.alloc);
		}

		[[nodiscard]] void* allocate(std::size_t size) {
			return alloc.allocate(size);
		}

		void deallocate(void* ptr, std::size_t size) {
			alloc.deallocate(ptr, size);
		}

		[[nodiscard]] void* reallocate(void* ptr, std::size_t old_size, std::size_t new_size) {
			return alloc.reallocate(ptr, old_size, new_size);
		}

		[[nodiscard]] void* allocate_hint(void* hint, std::size_t size) {
			return alloc.allocate_hint(hint, size);
		}

		[[nodiscard]] void* reallocate_hint(void* hint, void* old_ptr, std::size_t old_size, std::size_t new_size) {
			return alloc.reallocate_hint(hint, old_ptr, old_size, new_size);
		}

		const range_set_t& get_alloc_ranges() const {
			return alloc.get_alloc_ranges();
		}

		const range_set_t& get_ranges() const {
			return alloc.get_ranges();
		}

		std::size_t get_page_size() const {
			return alloc.get_page_size();
		}

	private:
		proxy_t& alloc;
	};

	template<class proxy_t>
	class scenario_alloc_t : public get_traits_t<proxy_t> {
	public:
		using tag_t = typename proxy_t::tag_t;
		using traits_t = typename proxy_t::traits_t;

		scenario_alloc_t(proxy_t& _alloc, request_stack_t _requests) : alloc(_alloc), requests(std::move(_requests)) {
			std::reverse(requests.begin(), requests.end());
		}

		void adopt(scenario_alloc_t& another) {
			alloc.adopt(another.alloc);
		}

	private:
		void check_requests() {
			if (requests.empty()) {
				std::cout << "no more reallocate requests" << std::endl;
				std::abort(); // TODO : exception
			}
		}

	public:
		[[nodiscard]] void* allocate(std::size_t size) {
			check_requests();

			auto request = requests.back();
			if (!request.is_alloc_request()) {
				std::cout << "current request is not an allocate request" << std::endl;
				return nullptr;
			} if (!request.matches_alloc_params(size)) {
				std::cout << "parameters given do not match expected request" << std::endl;
				return nullptr;
			} if (void* ptr = alloc.allocate_hint(request.hint, size)) {
				requests.pop_back();
				return ptr;
			} return nullptr;
		}

		void deallocate(void* ptr, std::size_t size) {
			alloc.deallocate(ptr, size);
		}

		[[nodiscard]] void* reallocate(void* ptr, std::size_t old_size, std::size_t new_size) {
			check_requests();
			
			auto request = requests.back();
			requests.pop_back();
			if (!request.is_realloc_request()) {
				std::cout << "current request is not a reallocate request" << std::endl;
				return nullptr;
			} if (!request.matches_realloc_params(ptr, old_size, new_size)) {
				std::cout << "parameters given do not match expected request" << std::endl;
				return nullptr;
			} if (void* ptr = alloc.reallocate_hint(request.hint, ptr, old_size, new_size)) {
				requests.pop_back();
				return ptr;
			} return nullptr;
		}

		[[nodiscard]] void* allocate_hint(void* hint, std::size_t size) {
			return alloc.allocate_hint(hint, size);
		}

		[[nodiscard]] void* reallocate_hint(void* hint, void* old_ptr, std::size_t old_size, std::size_t new_size) {
			return alloc.reallocate_hint(hint, old_ptr, old_size, new_size);	
		}

		const range_set_t& get_alloc_ranges() const {
			return alloc.get_alloc_ranges();
		}

		const range_set_t& get_ranges() const {
			return alloc.get_ranges();
		}

		std::size_t get_page_size() const {
			return alloc.get_page_size();
		}

	private:
		proxy_t& alloc; 
		request_stack_t requests;
	};

	template<class alloc_t>
	struct ranges_info_t {
		friend std::ostream& operator << (std::ostream& os, const ranges_info_t& info) {
			for (auto& range : info.alloc.get_ranges()) {
				os << range << std::endl;
			} return os;
		}

		alloc_t& alloc;
	};

	template<class alloc_t>
	struct alloc_ranges_info_t {
		friend std::ostream& operator << (std::ostream& os, const alloc_ranges_info_t& info) {
			for (auto& range : info.alloc.get_alloc_ranges()) {
				os << range << std::endl;
			} return os;
		}

		alloc_t& alloc;
	};

	struct block_size_t {
		constexpr operator std::ptrdiff_t() const {
			return mem::block_align * blocks;
		}

		std::ptrdiff_t blocks{};
	};

	struct block_mem_t {
		constexpr operator void*() const {
			return (char*)ptr + (std::ptrdiff_t)offset;
		}

		void* ptr{};
		block_size_t offset{};
	};

	struct block_view_t {
		void* ptr{};
		block_size_t size{};
	};

	std::ostream& operator << (std::ostream& os, const block_view_t& view) {
		ios_state_guard_t guard{os};
		unsigned char* ptr = (unsigned char*)view.ptr;
		for (std::size_t i = 0; i < view.size; i++) {
			if (i % mem::block_align == 0) {
				os << std::dec << std::setfill(' ') << std::setw(8) << i / mem::block_align << ": ";
			}
			os << std::hex << std::setfill('0') << std::setw(2) << (int)ptr[i];
			if ((i + 1) % mem::block_align == 0) {
				os << std::endl;
			}
		} return os;
	}

	void check_allocation(void* allocated, void* expected) {
		if (allocated != expected) {
			std::cerr << "allocation is invalid: expected " << expected << " but got " << allocated << std::endl;
			std::abort();
		}
	}
}