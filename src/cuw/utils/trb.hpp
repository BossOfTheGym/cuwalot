#pragma once

#include "bst.hpp"
#include "trb_node.hpp"

#include <cassert>
#include <utility>

namespace cuw::trb {
	template<class node_t>
	[[nodiscard]] node_t* fix_insert(node_t* root, node_t* node) {
		while (node->parent && get_color(node->parent) == node_color_t::Red) {
			if (node->parent == node->parent->parent->left) {
				node_t* uncle = node->parent->parent->right;
				if (uncle && get_color(uncle) == node_color_t::Red) {
					set_color(node->parent, node_color_t::Black);
					set_color(uncle, node_color_t::Black);
					set_color(node->parent->parent, node_color_t::Red);
					node = node->parent->parent;
				} else {
					if (node == node->parent->right) {
						node = node->parent;
						root = bst::rotate_left(root, node);
					}
					set_color(node->parent, node_color_t::Black);
					set_color(node->parent->parent, node_color_t::Red);
					root = bst::rotate_right(root, (node_t*)node->parent->parent);
				}
			} else {
				node_t* uncle = node->parent->parent->left;
				if (uncle && get_color(uncle) == node_color_t::Red) {
					set_color(node->parent, node_color_t::Black);
					set_color(uncle, node_color_t::Black);
					set_color(node->parent->parent, node_color_t::Red);
					node = node->parent->parent;
				} else {
					if (node == node->parent->left) {
						node = node->parent;
						root = bst::rotate_right(root, node);
					}
					set_color(node->parent, node_color_t::Black);
					set_color(node->parent->parent, node_color_t::Red);
					root = bst::rotate_left(root, (node_t*)node->parent->parent);
				}
			}
		}
		set_color(root, node_color_t::Black);
		return root;
	}

	// element will be inserted at the beginning of the range of equal elements
	// hint is some valid insert position (like gotten from search_{lb,ub} functions) 
	template<class node_t, class key_ops_t>
	[[nodiscard]] node_t* insert_lb_hint(node_t* root, node_t* node, node_t* hint, key_ops_t&& kops) {
		assert(node);

		node->parent = hint;
		if (!hint) {
			root = node;
		} else {
			if (kops.compare(kops.get_key(hint), kops.get_key(node))) {
				assert(!hint->right);
				hint->right = node;
			} else {
				assert(!hint->left);
				hint->left = node;
			}
		}
		node->left = nullptr;
		node->right = nullptr;
		set_color(node, node_color_t::Red);
		return fix_insert(root, node);
	}

	template<class node_t, class key_ops_t>
	[[nodiscard]] node_t* insert_lb(node_t* root, node_t* node, key_ops_t&& kops) {
		assert(node);

		auto [lb, prev] = bst::search_insert_lb(root, kops, kops.get_key(node));
		return insert_lb_hint(root, node, prev, std::forward<key_ops_t>(kops));
	}

	// element will be inserted at the end of the range of equal elements
	// hint is some valid insert position (like gotten from search_{lb,ub} functions)
	template<class node_t, class key_ops_t>
	[[nodiscard]] node_t* insert_ub_hint(node_t* root, node_t* node, node_t* hint, key_ops_t&& kops) {
		assert(node);

		node->parent = hint;
		if (!hint) {
			root = node;
		} else {
			if (!kops.compare(kops.get_key(node), kops.get_key(hint))) {
				assert(!hint->right);
				hint->right = node;
			} else {
				assert(!hint->left);
				hint->left = node;
			}
		}
		node->left = nullptr;
		node->right = nullptr;
		set_color(node, node_color_t::Red);
		return fix_insert(root, node);
	}

	template<class node_t, class key_ops_t>
	[[nodiscard]] node_t* insert_ub(node_t* root, node_t* node, key_ops_t&& kops) {
		assert(node);

		node_t* prev = bst::search_insert_ub(root, kops, kops.get_key(node));
		return insert_ub_hint(root, node, prev, std::forward<key_ops_t>(kops));
	}

	// insert node after given node, so tree order will be ... -> after -> node -> ...
	template<class node_t>
	[[nodiscard]] node_t* insert_after(node_t* root, node_t* after, node_t* node) {        
		assert(root);
		assert(after);
		assert(node);

		if (after->right) {
			node_t* succ = successor(after);
			succ->left = node;
			node->parent = succ;
		} else {
			after->right = node;
			node->parent = after;
		}
		node->left = nullptr;
		node->right = nullptr;
		set_color(node, node_color_t::Red);
		return fix_insert(root, node);
	}

	// insert node before given node, so tree order will be ... -> node -> before -> ...
	template<class node_t>
	[[nodiscard]] node_t* insert_before(node_t* root, node_t* before, node_t* node) {
		assert(root);
		assert(before);
		assert(node);

		if (before->left) {
			node_t* pred = bst::predecessor(before);
			pred->right= node;
			node->parent = pred;
		} else {
			before->left = node;
			node->parent = before;
		}
		node->left = nullptr;
		node->right = nullptr;
		set_color(node, node_color_t::Red);
		return fix_insert(root, node);
	}

	template<class node_t>
	[[nodiscard]] node_t* fix_remove(node_t* root, node_t* restore_parent, node_t* restore, bool restore_parent_left) {
		if (!restore && restore != root) { // nullptr is colored black
			if (restore_parent_left) {
				node_t* brother = restore_parent->right; // brother must not be nullptr
				if (get_color(brother) == node_color_t::Red) {
					set_color(brother, node_color_t::Black);
					set_color(restore_parent, node_color_t::Red);
					root = bst::rotate_left(root, restore_parent);
					brother = restore_parent->right; // must not be nullptr
				}

				// left or (and) right can be nullptr
				if ((!brother->left || get_color(brother->left) == node_color_t::Black)
					&& (!brother->right || get_color(brother->right) == node_color_t::Black)) {
					set_color(brother, node_color_t::Red);
					restore = restore_parent;
				} else { // one or two children are red
					if (!brother->right || get_color(brother->right) == node_color_t::Black) {
						set_color(brother->left, node_color_t::Black);
						set_color(brother, node_color_t::Red);
						root = bst::rotate_right(root, brother);
						brother = restore_parent->right;
					}
					set_color(brother, get_color(restore_parent));
					set_color(restore_parent, node_color_t::Black);
					set_color(brother->right, node_color_t::Black);
					root = bst::rotate_left(root, restore_parent);
					restore = root;
				}
			} else {
				node_t* brother = restore_parent->left;
				if (get_color(brother) == node_color_t::Red) {
					set_color(brother, node_color_t::Black);
					set_color(restore_parent, node_color_t::Red);
					root = bst::rotate_right(root, restore_parent);
					brother = restore_parent->left;
				}

				if ((!brother->left || get_color(brother->left) == node_color_t::Black)
					&& (!brother->right || get_color(brother->right) == node_color_t::Black)) {
					set_color(brother, node_color_t::Red);
					restore = restore_parent;
				} else {
					if (!brother->left || get_color(brother->left) == node_color_t::Black) {
						set_color(brother->right, node_color_t::Black);
						set_color(brother, node_color_t::Red);
						root = bst::rotate_left(root, brother);
						brother = restore_parent->left;
					}
					set_color(brother, get_color(restore_parent));
					set_color(restore_parent, node_color_t::Black);
					set_color(brother->left, node_color_t::Black);
					root = bst::rotate_right(root, restore_parent);
					restore = root;
				}
			}
		} if (!restore) {
			return root; // I guess, it must be nullptr
		}

		while (restore != root && get_color(restore) == node_color_t::Black) {
			if (restore == restore->parent->left) {
				node_t* brother = restore->parent->right;
				if (get_color(brother) == node_color_t::Red) {
					set_color(brother, node_color_t::Black);
					set_color(restore->parent, node_color_t::Red);
					root = bst::rotate_left(root, (node_t*)restore->parent);
					brother = restore->parent->right;
				}

				if ((!brother->left || get_color(brother->left) == node_color_t::Black)
					&& (!brother->right || get_color(brother->right) == node_color_t::Black)) {
					set_color(brother, node_color_t::Red);
					restore = restore->parent;
				} else {
					if (!brother->right || get_color(brother->right) == node_color_t::Black) {
						set_color(brother->left, node_color_t::Black);
						set_color(brother, node_color_t::Red);
						root = bst::rotate_right(root, brother);
						brother = restore->parent->right;
					}
					set_color(brother, get_color(restore->parent));
					set_color(restore->parent, node_color_t::Black);
					set_color(brother->right, node_color_t::Black);
					root = bst::rotate_left(root, (node_t*)restore->parent);
					restore = root;
				}
			} else {
				node_t* brother = restore->parent->left;
				if (get_color(brother) == node_color_t::Red) {
					set_color(brother, node_color_t::Black);
					set_color(restore->parent, node_color_t::Red);
					root = bst::rotate_right(root, (node_t*)restore->parent);
					brother = restore->parent->left;
				}

				if ((!brother->left || get_color(brother->left) == node_color_t::Black)
					&& (!brother->right || get_color(brother->right) == node_color_t::Black)) {
					set_color(brother, node_color_t::Red);
					restore = restore->parent;
				} else {
					if (!brother->left || get_color(brother->left) == node_color_t::Black) {
						set_color(brother->right, node_color_t::Black);
						set_color(brother, node_color_t::Red);
						root = bst::rotate_left(root, brother);
						brother = restore->parent->left;
					}
					set_color(brother, get_color(restore->parent));
					set_color(restore->parent, node_color_t::Black);
					set_color(brother->left, node_color_t::Black);
					root = bst::rotate_right(root, (node_t*)restore->parent);
					restore = root;
				}
			}
		}
		set_color(restore, node_color_t::Black);
		return root;
	}

	template<class node_t>
	[[nodiscard]] node_t* remove(node_t* root, node_t* node) {
		assert(node);

		node_color_t removed_color = get_color(node);

		node_t* restore_parent = nullptr;
		node_t* restore = nullptr;
		bool restore_parent_left = false;
		if (!node->left) {
			restore = node->right;
			restore_parent = node->parent;
			if (node->parent) {
				restore_parent_left = node->parent->left == node;
			} root = bst::transplant(root, node, node->right);
		} else if (!node->right) {
			restore = node->left;
			restore_parent = node->parent;
			if (node->parent) {
				restore_parent_left = node->parent->left == node;
			} root = bst::transplant(root, node, node->left);
		} else {
			node_t* leftmost = bst::tree_min(node->right);
			removed_color = get_color(leftmost);
			restore = leftmost->right;
			if (node->right == leftmost) {
				restore_parent = leftmost;
				restore_parent_left = false;
			} else {
				restore_parent = leftmost->parent;
				restore_parent_left = true;
				root = bst::transplant(root, leftmost, leftmost->right);
				leftmost->right = node->right;
				node->right->parent = leftmost;
			}
			leftmost->left = node->left;
			node->left->parent = leftmost;
			root = bst::transplant(root, node, leftmost);
			set_color(leftmost, get_color(node));
		}

		if (removed_color == node_color_t::Black) {
			root = fix_remove(root, restore_parent, restore, restore_parent_left);
		} return root;
	}

	namespace impl {
		// returns (black height, check value)
		template<class node_t>
		std::tuple<int, bool> check_rb_inv(node_t* root) {
			if (!root) {
				return {1, true};
			}

			auto [bh_left, check_left] = check_rb_inv(root->left);
			if (!check_left) {
				return {-1, false};
			}

			auto [bh_right, check_right] = check_rb_inv(root->right);
			if (!check_right) {
				return {-1, false};
			}

			if (bh_left != bh_right) {
				return {-1, false};
			}

			int bh = bh_left;
			if (get_color(root) == node_color_t::Black) {
				bh++;
			} return {bh, true};
		}
	}

	// serves mostly for debug purposes, checks if all black heights are equal
	template<class node_t>
	bool check_rb_invariant(node_t* root) {
		auto [bh, check] = impl::check_rb_inv(root);
		return check;
	}
}