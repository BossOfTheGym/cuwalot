#pragma once

#include "core.hpp"

namespace cuw::mem {
	template<class __entry_t>
	class list_cache_t {
	public:
		using entry_t = __entry_t;

		list_cache_t() {
			list::init(&entry);
		}

		list_cache_t(list_cache_t&& another) noexcept {
			list::init(&entry);
			*this = std::move(another);
		}

		list_cache_t& operator = (list_cache_t&& another) noexcept {
			if (this != &another) { // best fucking practice if you have paranoia like I do
				list::move_head(&another.entry, &entry);
			} return *this;
		}
 
		list_cache_t(const list_cache_t&) = delete;
		list_cache_t& operator = (const list_cache_t&) = delete;

		void insert(entry_t* node) {
			list::insert_after(&entry, node);
		}

		// node not neccessarily belongs to the list cache
		// it may belong to another 
		void reinsert(entry_t* node) {
			list::erase(node);
			list::insert_after(&entry, node);
		}

		void erase(entry_t* node) {
			list::erase(node);
		}

		entry_t* peek() const {
			if (!list::empty(&entry)) {
				return entry.next;
			} return nullptr;
		}

		void adopt(list_cache_t& another) {
			list::append(&entry, &another.entry);
		}

		void split(int first, list_cache_t& part1, list_cache_t& part2) {
			list::split(&entry, first, &part1.entry, &part2.entry);
		}

		// bool func(entry_t*)
		template<class func_t>
		void release_all(func_t func) {
			entry_t* head = &entry;
			entry_t* prev = head;
			entry_t* curr = head->next;
			while (curr != head) {
				entry_t* next = curr->next;
				if (func(curr)) {
					list::link(prev, next);
				} else {
					prev = curr;
				} curr = next;
			}
		}

		void reset() {
			list::init(&entry);
		}

		auto begin() {
			return list::begin(&entry);
		}

		auto end() {
			return list::end(&entry);
		}

	private:
		entry_t entry{};
	};	
}