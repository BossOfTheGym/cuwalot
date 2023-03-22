#pragma once

#include <cstddef>
#include <utility>

namespace cuw::list {
	// everywhere in the functions below prev and next are not nullptr
	template<class __data_t>
	struct node_t {
		using data_t = __data_t;

		node_t* prev;
		node_t* next;
		data_t data{};
	};

	template<>
	struct node_t<void> {
		using data_t = void;

		node_t* prev;
		node_t* next;
	};

	struct entry_t {
		entry_t* prev;
		entry_t* next;
	};

	template<class node_t>
	void init(node_t* head) {
		head->prev = head;
		head->next = head;
	}

	template<class node_t>
	bool empty(const node_t* head) {
		return head == head->next;
	}

	template<class node_t>
	void link(node_t* first, node_t* second) {
		first->next = second;
		second->prev = first;
	}

	template<class node_t>
	void insert_before(node_t* before, node_t* node) {
		node->prev = before->prev;
		node->next = before;
		node->prev->next = node;
		node->next->prev = node;
	}

	template<class node_t>
	void insert_after(node_t* after, node_t* node) {
		node->prev = after;
		node->next = after->next;
		node->next->prev = node;
		node->prev->next = node;
	}

	// do not erase head(well, you temporarly can like when swapping nodes)!
	template<class node_t>
	void erase(node_t* node) {
		node->prev->next = node->next;
		node->next->prev = node->prev;
	}

	template<class node_t>
	void swap_prev(node_t* node) {
		node_t* prev = node->prev;
		erase(prev);
		insert_after(node, prev);
	}

	template<class node_t>
	void swap_next(node_t* node) {
		node_t* next = node->next;
		erase(next);
		insert_before(node, next);
	}

	// do not swap nodes from different lists, please
	template<class node_t>
	void swap_nodes(node_t* node1, node_t* node2) {
		if (node1 == node2) {
			return; // early out
		} if (node1->next == node2) {
			swap_next(node1);
			return;
		} if (node2->next == node1) {
			swap_next(node2);
			return;
		}
		std::swap(node1->prev, node2->prev);
		std::swap(node1->next, node2->next);
		node1->prev->next = node1;
		node1->next->prev = node1;
		node2->prev->next = node2;
		node2->next->prev = node2;
	}

	// another is empty after function call
	template<class node_t>
	void append(node_t* list, node_t* another) {
		if (empty(another)) {
			return;
		}
		list->prev->next = another->next;
		another->next->prev = list->prev;
		another->prev->next = list;
		list->prev = another->prev;
		init(another);
	}

	// src & dst are list heads
	// src must not be empty, dst can
	template<class node_t>
	void move_head_not_empty(node_t* src, node_t* dst) {
		dst->prev = src->prev;
		dst->next = src->next;
		dst->prev->next = dst;
		dst->next->prev = dst;
		init(src);
	}

	// src & dst are list heads (at least assumed to be)
	// safe to call if src is empty
	// src is empty after function call
	template<class node_t>
	void move_head(node_t* src, node_t* dst) {
		if (!empty(src)) {
			move_head_not_empty(src, dst);
		} else {
			init(dst);
		}
	}

	template<class node_t>
	void swap_heads(node_t* src, node_t* dst) {
		if (empty(src)) {
			move_head(dst, src);
			return;
		} if (empty(dst)) {
			move_head(src, dst);
			return;
		}
		std::swap(src->prev, dst->prev);
		std::swap(src->next, dst->next);
		src->prev->next = src;
		src->next->prev = src;
		dst->prev->next = dst;
		dst->next->prev = dst;
	}


	// part1, part2 are either empty or contain something after function call
	// list is empty after function call
	template<class node_t, class int_t>
	void split(node_t* list, int_t first_part_size, node_t* part1, node_t* part2) {
		if (empty(list)) {
			init(part1);
			init(part2);
			return;
		}

		node_t* head = list;
		node_t* curr = head->next;
		for (int_t i = 0; i < first_part_size && curr != head; i++) {
			curr = curr->next;
		}

		// first_part_size == 0
		if (curr == head->next) {
			move_head(list, part2); // all nodes
			init(part1); // empty
			return;
		}

		// list is smaller than first_part_size
		if (curr == head) {
			move_head(list, part1); // all nodes
			init(part2); // empty
			return;
		}

		part1->next = head->next;
		part1->prev = curr->prev;
		curr->prev->next = part1;
		head->next->prev = part1;

		part2->next = curr;
		part2->prev = head->prev;
		curr->prev = part2;
		head->prev->next = part2;

		init(list);
	}

	// can be used in ranged for loop
	// returns node pointer on dereference
	// not stl-usable in the most cases
	template<class __node_t>
	class iter_t {
	public:
		using node_t = __node_t;

		using iterator_category = std::bidirectional_iterator_tag;
		using value_type = node_t*;
		using pointer = value_type*;
		using reference = value_type&;
		using difference_type = std::ptrdiff_t;

		iter_t(node_t* value) : node{value} {}
		iter_t(const iter_t&) = default;
		
		bool operator == (const iter_t&) const = default;

		value_type operator * () {
			return node;
		}

		iter_t& operator ++ () {
			node = node->next;
			return *this;
		}

		iter_t operator ++ (int) {
			iter_t iter = *this;
			return ++(*this), iter;
		}

		iter_t& operator -- () {
			node = node->prev;
			return *this;
		}

		iter_t operator -- (int) {
			iter_t iter = *this;
			return --(*this), iter;
		}

	private:
		node_t* node{};
	};

	template<class node_t>
	auto begin(node_t* head) {
		return iter_t{head->next};
	}

	template<class node_t>
	auto cbegin(const node_t* head) {
		return iter_t<const node_t>{head->next};
	}

	template<class node_t>
	auto end(node_t* head) {
		return iter_t{head};
	}

	template<class node_t>
	auto cend(const node_t* head) {
		return iter_t<const node_t>{head};
	}

	template<class node_t>
	struct iter_helper_t {
		auto begin() {
			return list::begin(head);
		}

		auto end() {
			return list::end(head);
		}

		node_t* head;
	};
}