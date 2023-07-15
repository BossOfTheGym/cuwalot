#pragma once

#include <new>
#include <memory>
#include <utility>
#include <cassert>
#include <cstddef>

template<class __type_t, std::size_t __max_chunks>
class linear_storage_t {
public:
	static constexpr std::size_t max_chunks = __max_chunks;

	using type_t = __type_t;

	union entry_t {
		entry_t() {}

		alignas(type_t) std::byte data[sizeof(type_t)];
		type_t dummy;
	};

	type_t* construct(std::size_t index, const type_t& elem) {
		assert(index < max_chunks);
		return new (&entries[index]) type_t(elem);
	}

	type_t* construct(std::size_t index, type_t&& elem) {
		assert(index < max_chunks);
		return new (&entries[index]) type_t(std::move(elem));
	}

	template<class ... args_t>
	type_t* emplace(std::size_t index, args_t&& ... args) {
		assert(index < max_chunks);
		if constexpr(std::is_aggregate_v<type_t>) {
			return new (&entries[index]) type_t{std::forward<args_t>(args)...};
		} else {
			return new (&entries[index]) type_t(std::forward<args_t>(args)...);
		}
	}

	void destroy(std::size_t index) {
		std::destroy_at(std::launder(reinterpret_cast<type_t*>(&entries[index])));
	}

	type_t& operator [] (std::size_t index) {
		assert(index < max_chunks);
		return *std::launder(reinterpret_cast<type_t*>(&entries[index]));
	}

	const type_t& operator [] (std::size_t index) const {
		assert(index < max_chunks);
		return *std::launder(reinterpret_cast<const type_t*>(&entries[index]));
	}

	type_t* data() {
		return &((*this)[0]);
	}

	const type_t* data() const {
		return &((*this)[0]);
	}

	std::size_t capacity() const {
		return max_chunks;
	}

private:
	entry_t entries[max_chunks];
};

template<class __type_t, std::size_t __max_chunks>
class pool_storage_t {
public:
	static constexpr std::size_t max_chunks = __max_chunks; 

	using type_t = __type_t;

	union entry_t {
		entry_t() {}

		alignas(type_t) std::byte data[sizeof(type_t)];
		void* next;
		type_t dummy;
	};

private:
	bool try_get_head_chunk() {
		if (!head) {
			if (allocated < max_chunks) {
				entry_t* entry = &entries[allocated++];
				entry->next = nullptr;
				head = entry;
			} else {
				return false;
			}
		}
		return true;
	}

public:
	type_t* construct(const type_t& elem) {
		if (!try_get_head_chunk()) {
			return nullptr;
		}
		void* next = std::launder(reinterpret_cast<entry_t*>(head))->next;
		type_t* obj = new (head) type_t(elem);
		head = next;
		return obj;
	}

	type_t* construct(type_t&& elem) {
		if (!try_get_head_chunk()) {
			return nullptr;
		}
		void* next = std::launder(reinterpret_cast<entry_t*>(head))->next;
		type_t* obj = new (head) type_t(std::move(elem));
		head = next;
		return obj;
	}

	template<class ... args_t>
	type_t* emplace(args_t&& ... args) {
		if (!try_get_head_chunk()) {
			return nullptr;
		}
		
		void* next = std::launder(reinterpret_cast<entry_t*>(head))->next;
		type_t* obj;
		if constexpr(std::is_aggregate_v<type_t>) {
			obj = new (head) type_t{std::forward<args_t>(args)...};
		} else {
			obj = new (head) type_t(std::forward<args_t>(args)...);
		}
		head = next;
		return obj;
	}

	void destroy(type_t* obj) {
		assert(has_obj(obj));
		std::destroy_at(obj);
		entry_t* entry = std::launder(reinterpret_cast<entry_t*>(obj));
		entry->next = head;
		head = obj;
		allocated--;
	}

	bool has_obj(type_t* obj) const {
		assert(((std::uintptr_t)obj & (alignof(entry_t) - 1)) == 0);
		auto diff = reinterpret_cast<entry_t*>(obj) - entries;
		return diff >= 0 && diff < max_chunks;
	}

	std::size_t capacity() const {
		return max_chunks;
	}

private:
	entry_t entries[max_chunks];
	void* head{};
	std::size_t allocated{};
};