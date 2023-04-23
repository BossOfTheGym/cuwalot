#pragma once

#include <cassert>

namespace cuw::bst {
	// parent pointer is treated specially on assignment as a little workaround for trb & tagged_ptr implementation
	// so only pointer part is assigned and not data part

	template<class node_t>
	node_t* tree_min(node_t* root) {
		assert(root);
		while (root->left) {
			root = root->left;
		} return root;
	}

	template<class node_t>
	node_t* tree_max(node_t* root) {
		assert(root);
		while (root->right) {
			root = root->right;
		} return root;
	}

	template<class node_t>
	node_t* predecessor(node_t* node) {
		assert(node);

		if (node->left) {
			return tree_max(node->left);
		}

		node_t* pred = node->parent;
		while (pred && node == pred->left) {
			node = pred;
			pred = pred->parent;
		} return pred;
	}

	template<class node_t>
	node_t* successor(node_t* node) {
		assert(node);

		if (node->right) {
			return tree_min(node->right);
		}

		node_t* succ = node->parent;
		while (succ && node == succ->right) {
			node = succ;
			succ = succ->parent;
		} return succ;
	}

	template<class node_t, class key_ops_t, class key_t>
	node_t* lower_bound(node_t* root, key_ops_t&& kops, key_t&& key) {
		node_t* lb = nullptr;
		node_t* curr = root;
		while (curr) {
			if (kops.compare(kops.get_key(curr), key)) {
				curr = curr->right;
			} else {
				lb = curr;
				curr = curr->left;
			}
		} return lb;
	}

	template<class node_t, class key_ops_t, class key_t>
	node_t* upper_bound(node_t* root, key_ops_t&& kops, key_t&& key) {
		node_t* ub = nullptr;
		node_t* curr = root;
		while (curr) {
			if (!kops.compare(key, kops.get_key(curr))) {
				curr = curr->right;
			} else {
				ub = curr;
				curr = curr->left;
			}
		} return ub;
	}

	template<class node_t>
	[[nodiscard]] node_t* rotate_left(node_t* root, node_t* node) {
		assert(node);
		assert(node->right);

		node_t* pivot = node->right;

		node->right = pivot->left;

		pivot->left = node;
		if (node->right) {
			node->right->parent = node;
		}

		if (node->parent) {
			if (node == node->parent->left) {
				node->parent->left = pivot;
			} else {
				node->parent->right = pivot;
			}
		} else {
			root = pivot;
		}

		pivot->parent = (node_t*)node->parent; // assign possibly tagged pointer, explicitly cast it to node_t*
		node->parent = pivot;
		return root;
	}

	template<class node_t>
	[[nodiscard]] node_t* rotate_right(node_t* root, node_t* node) {
		assert(node);
		assert(node->left);

		node_t* pivot = node->left;

		node->left = pivot->right;
		pivot->right = node;
		if (node->left) {
			node->left->parent = node;
		}

		if (node->parent) {
			if (node == node->parent->left) {
				node->parent->left = pivot;
			} else {
				node->parent->right = pivot;
			}
		} else {
			root = pivot;
		}

		pivot->parent = (node_t*)node->parent; // assign possibly tagged pointer, explicitly cast it to node_t*
		node->parent = pivot;
		return root;
	}

	template<class node_t>
	struct search_res_t {
		node_t* bound{};
		node_t* prev{};
	};

	// result can be used to test if node with given key exists in tree
	template<class node_t, class key_ops_t, class key_t>
	search_res_t<node_t> search_insert_lb(node_t* root, key_ops_t&& kops, key_t&& key) {
		node_t* lb = nullptr;
		node_t* prev = nullptr;
		node_t* curr = root;
		while (curr) {
			prev = curr;
			if (kops.compare(kops.get_key(curr), key)) {
				curr = curr->right;
			} else {
				lb = curr;
				curr = curr->left;
			}
		} return {lb, prev};
	}

	// result can be used to insert node only, check if key exists in tree is impossible
	template<class node_t, class key_ops_t, class key_t>
	node_t* search_insert_ub(node_t* root, key_ops_t&& kops, key_t&& key) {
		node_t* prev = nullptr;
		node_t* curr = root;
		while (curr) {
			prev = curr;
			if (!kops.compare(key, kops.get_key(curr))) {
				curr = curr->right;
			} else {
				curr = curr->left;
			}
		} return prev;
	}

	template<class node_t>
	[[nodiscard]] node_t* transplant(node_t* root, node_t* node, node_t* trans) {
		if (node->parent) {
			if (node == node->parent->left) {
				node->parent->left = trans;
			} else {
				node->parent->right = trans;
			}
		} else {
			root = trans;
		} if (trans) {
			trans->parent = (node_t*)node->parent; // assign possibly tagged pointer, explicitly cast it to node_t*
		} return root;
	}

	// we already copied state from old_node to the new_node
	template<class node_t>
	[[nodiscard]] node_t* update_links(node_t* root, node_t* new_node, node_t* old_node) {
		assert(new_node);
		assert(old_node);
		if (new_node->parent) {
			if (new_node->parent->left == old_node) {
				new_node->parent->left = new_node;
			} else {
				new_node->parent->right = new_node;
			}
		} else {
			root = new_node;
		} if (new_node->left) {
			new_node->left->parent = new_node;
		} if (new_node->right) {
			new_node->right->parent = new_node;
		} return root;
	}

	template<class node_t, class func_t>
	void traverse_inorder(node_t* root, func_t&& func) {
		if (root) {
			auto left = root->left, right = root->right;
			traverse_inorder(left, func);
			func(root);
			traverse_inorder(right, func);
		}
	}

	template<class node_t, class func_t>
	void traverse_preorder(node_t* root, func_t&& func) {
		if (root) {
			auto left = root->left, right = root->right;
			func(root);
			traverse_preorder(left, func);
			traverse_preorder(right, func);
		}
	}

	template<class node_t, class func_t>
	void traverse_postorder(node_t* root, func_t&& func) {
		if (root) {
			auto left = root->left, right = root->right;
			traverse_postorder(left, func);
			traverse_postorder(right, func);
			func(root);
		}
	}

	// creates singly-linked list, left-pointer is next-pointer
	template<class node_t>
	struct head_tail_t {
		void append(const head_tail_t& another) {
			if (!another.head) {
				return;
			}
			
			if (head) {
				tail->left = another.head;
				tail = another.tail;
			} else {
				head = another.head;
				tail = another.tail;
			}
		}

		void append(node_t* node) {
			assert(node);

			if (head) {
				tail->left = node;
				tail = node;
			} else {
				head = node;
				tail = node;
			} node->left = nullptr;
		}

		node_t* head{};
		node_t* tail{};
	};

	template<class node_t>
	head_tail_t<node_t> flatten(node_t* root) {
		head_tail_t<node_t> head_tail{};
		if (root) {
			node_t* left = root->left;
			node_t* right = root->right;
			head_tail.append(flatten(left));
			head_tail.append(root);
			head_tail.append(flatten(right));
		} return head_tail;
	}

	template<class __node_t>
	class tree_iter_t {
	public:
		using node_t = __node_t;

		using iterator_category = std::bidirectional_iterator_tag;
		using value_type = node_t*;
		using pointer = value_type*;
		using reference = value_type&;
		using difference_type = std::ptrdiff_t;

		tree_iter_t(node_t* value) : node{value} {}

		bool operator == (const tree_iter_t&) const = default;

		value_type operator * () {
			return node;
		}

		tree_iter_t& operator -- () {
			node = predecessor(node);
			return *this;
		}

		tree_iter_t operator -- (int) {
			auto iter = *this;
			return --(*this), iter;
		}

		tree_iter_t& operator ++ () {
			node = successor(node);
			return *this;
		}

		tree_iter_t operator ++ (int) {
			auto iter = *this;
			return ++(*this), iter;
		}

	private:
		node_t* node{};
	};

	template<class node_t>
	auto begin(node_t* root) {
		return tree_iter_t(root ? tree_min(root) : nullptr);
	}

	template<class node_t>
	auto end(node_t* root) {
		return tree_iter_t((node_t*)nullptr);
	}

	template<class node_t>
	struct tree_wrapper_t {
		auto begin() {
			return bst::begin(root); 
		}

		auto end() {
			return bst::end(root);
		}

		node_t* root{};
	};
}