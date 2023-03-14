#pragma once

#include <utility>
#include <type_traits>

#include "ptr.hpp"

namespace cuw::trb {
	enum class node_color_t : std::uintptr_t {
		Black = 0,
		Red = 1,
	};

	template<class traits_t>
	using traits_data_t = typename traits_t::data_t;

	// default tag applied to a tree node
	struct default_node_tag_t;

	// packed node declaration
	// color attribute is packed into parent pointer

	// node struct must be aligned by at least 4 bytes(2 zero bits) on 32 bit system
	// and by at least 8 bytes(3 free bits) on 64 bit system
	inline constexpr std::size_t default_bits = 2;

	namespace impl {
		template<class __data_t>
		struct dummy_node_t {
			using data_t = __data_t;
			tagged_ptr_t<dummy_node_t, 0> parent{};
			dummy_node_t* left;
			dummy_node_t* right;
			data_t data;
		};

		template<>
		struct dummy_node_t<void> {
			using data_t = void;
			tagged_ptr_t<dummy_node_t, 0> parent{};
			dummy_node_t* left;
			dummy_node_t* right;
		};
	}

	template<class data_t>
	inline constexpr std::size_t tree_node_packed_align_v = alignof(impl::dummy_node_t<data_t>);

	template<class __data_t, std::size_t __align = tree_node_packed_align_v<__data_t>, std::size_t __bits = default_bits>
	struct tree_node_packed_traits_t {
		static constexpr std::size_t bits = __bits;
		static constexpr std::size_t align = std::max(tree_node_packed_align_v<__data_t>, std::max(__align, (std::size_t)1 << __bits));
		using data_t = __data_t;
	};

	// primary template
	template<class __traits_t, class __tag_t = default_node_tag_t, class = traits_data_t<__traits_t>>
	struct alignas(__traits_t::align) tree_node_packed_t {
		using tag_t = __tag_t;
		using traits_t = __traits_t;
		using data_t = traits_data_t<traits_t>;
		using tagged_node_t = tagged_ptr_t<tree_node_packed_t, traits_t::bits>; // nothing common with tag_t!

		tagged_node_t parent;
		tree_node_packed_t* left;
		tree_node_packed_t* right;
		data_t data;
	};

	// specialization if data is void
	template<class __traits_t, class __tag_t>
	struct alignas(__traits_t::align) tree_node_packed_t<__traits_t, __tag_t, void> {
		using tag_t = __tag_t;
		using traits_t = __traits_t;
		using data_t = traits_data_t<traits_t>;
		using tagged_node_t = tagged_ptr_t<tree_node_packed_t, traits_t::bits>;

		tagged_node_t parent;
		tree_node_packed_t* left;
		tree_node_packed_t* right;
	};

	// little helpers
	namespace impl {
		template<class type_t, std::size_t bits>
		auto tagged_ptr_test(const tagged_ptr_t<type_t, bits>&) -> std::true_type;

		template<class type_t, std::size_t bits>
		auto tagged_ptr_test(tagged_ptr_t<type_t, bits>&&) -> std::true_type;

		template<class whatever>
		auto tagged_ptr_test(whatever) -> std::false_type;

		template<class node_ptr_t>
		struct is_tagged_ptr_t : decltype(tagged_ptr_test(std::declval<node_ptr_t>())) {};
	}

	template<class node_ptr_t>
	inline constexpr bool is_tagged_ptr_v = impl::is_tagged_ptr_t<node_ptr_t>::value;

	template<class node_ptr_t>
	using enable_if_tagged_ptr_t = std::enable_if_t<is_tagged_ptr_v<node_ptr_t>, node_ptr_t>;

	template<class traits_t, class tag_t>
	inline node_color_t get_color(tree_node_packed_t<traits_t, tag_t>* node) {
		return (node_color_t)node->parent.get_data();
	}

	template<class node_ptr_t, class = enable_if_tagged_ptr_t<node_ptr_t>>
	inline node_color_t get_color(node_ptr_t node) {
		return get_color(node.get_ptr());
	}

	template<class traits_t, class tag_t>
	inline void set_color(tree_node_packed_t<traits_t, tag_t>* node, node_color_t color) {
		node->parent.set_data((std::uintptr_t)color);
	}

	template<class node_ptr_t, class = enable_if_tagged_ptr_t<node_ptr_t>>
	inline void set_color(node_ptr_t node, node_color_t color) {
		set_color(node.get_ptr(), color);
	}

	namespace impl {
		template<class traits_t>
		auto tree_node_packed_test(const tree_node_packed_t<traits_t>&) -> std::true_type;

		template<class traits_t>
		auto tree_node_packed_test(tree_node_packed_t<traits_t>&&) -> std::true_type;

		template<class whatever>
		auto tree_node_packed_test(whatever) -> std::false_type;

		template<class node_t>
		struct is_tree_node_packed_t : decltype(tree_node_packed_test(std::declval<node_t>())) {};
	}

	template<class node_t>
	inline constexpr bool is_tree_node_packed_v =
		impl::is_tree_node_packed_t<std::remove_volatile_t<std::remove_pointer_t<node_t>>>::value;

	template<class node_t, class alloc_t, class ... args_t>
	inline auto alloc_node(alloc_t alloc, args_t&& ... args) -> std::enable_if_t<is_tree_node_packed_v<node_t>, node_t*> {
		return alloc(nullptr, nullptr, nullptr, std::forward<args_t>(args)...);
	}


	// simple node declaration
	template<class __data_t>
	struct tree_node_traits_t {
		using data_t = __data_t;
	};

	// primary template
	template<class __traits_t, class __tag_t = default_node_tag_t, class = traits_data_t<__traits_t>>
	struct tree_node_t {
		using tag_t = __tag_t;
		using traits_t = __traits_t;
		using data_t = traits_data_t<traits_t>;

		tree_node_t* parent;
		tree_node_t* left;
		tree_node_t* right;
		std::uintptr_t attrs; // first bit is color
		data_t data;
	};

	// specialization if data is void
	template<class __traits_t, class __tag_t>
	struct tree_node_t<__traits_t, __tag_t, void> {
		using tag_t = __tag_t;
		using traits_t = __traits_t;
		using data_t = traits_data_t<traits_t>;

		tree_node_t* parent;
		tree_node_t* left;
		tree_node_t* right;
		std::uintptr_t attrs; // first bit is color
	};

	template<class traits_t, class tag_t>
	inline node_color_t get_color(tree_node_t<traits_t, tag_t>* node) {
		return (node_color_t)(node->attrs & 0x1);
	}

	template<class traits_t, class tag_t>
	inline void set_color(tree_node_t<traits_t, tag_t>* node, node_color_t color) {
		node->attrs = (node->attrs & ~0x1) | (std::uintptr_t)color;
	}

	namespace impl {
		template<class traits_t>
		auto tree_node_test(const tree_node_t<traits_t>&) -> std::true_type;

		template<class traits_t>
		auto tree_node_test(tree_node_t<traits_t>&&) -> std::true_type;

		template<class whatever>
		auto tree_node_test(whatever) -> std::false_type;

		template<class node_t>
		struct is_tree_node_t : decltype(tree_node_test(std::declval<node_t>())) {};
	}

	template<class node_t>
	inline constexpr bool is_tree_node_v =
		impl::is_tree_node_t<std::remove_volatile_t<std::remove_pointer_t<node_t>>>::value;

	template<class node_t, class alloc_t, class ... args_t>
	inline auto alloc_node(alloc_t alloc, args_t&& ... args) -> std::enable_if_t<is_tree_node_v<node_t>, node_t*> {
		return alloc(nullptr, nullptr, nullptr, (std::uintptr_t)0, std::forward<args_t>(args)...);
	}

	// pointer to node can serve as implicit key
	template<class __node_t>
	struct implicit_key_t {
		using node_t = __node_t;
		
		node_t* get_key(node_t* node) const {
			return node;
		}
	};
}