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

		void insert_back(entry_t* node) {
			list::insert_before(&entry, node);
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
		entry_t* find(func_t func) {
			entry_t* head = &entry;
			entry_t* curr = head->next;
			while (curr != head) {
				if (func(curr)) {
					return curr;					
				} curr = curr->next;
			} return nullptr;
		}

		// void func(entry_t*)
		template<class func_t>
		void traverse(func_t func) {
			entry_t* head = &entry;
			entry_t* curr = head->next;
			while (curr != head) {
				entry_t* next = curr->next;
				func(curr);
				curr = next;
			}
		}

		// bool func(entry_t*)
		// returns how many entries were released
		template<class func_t>
		int release_all(func_t func) {
			entry_t* head = &entry;
			entry_t* prev = head;
			entry_t* curr = head->next;
			int released = 0;
			while (curr != head) {
				entry_t* next = curr->next;
				if (func(curr)) {
					list::link(prev, next);
					++released;
				} else {
					prev = curr;
				} curr = next;
			} return released;
		}

		void reset() {
			list::init(&entry);
		}

		auto begin() {
			return list::begin(&entry);
		}

		auto begin() const {
			return list::cbegin(&entry);
		}

		auto end() {
			return list::end(&entry);
		}

		auto end() const {
			return list::cend(&entry);
		}

	private:
		entry_t entry{};
	};	
}