#include <iostream>

#include <cuw/utils/list.hpp>

using namespace cuw;

namespace {
	struct list_t {
		list_t* prev{};
		list_t* next{};
		int data{};
	};

	void create_list(list_t& head, int count) {
		list::init(&head);
		for (int i = 0; i < count; i++) {
			list_t* node = new list_t{ .data = i };
			list::insert_before(&head, node);
		}
	}

	void destroy_list(list_t& head) {
		list_t* curr = head.next;
		while (curr != &head) {
			list_t* next = curr->next;
			delete curr;
			curr = next;
		}
	}

	template<class list_t>
	struct print_list_t {
		friend std::ostream& operator << (std::ostream& os, const print_list_t& pl) {
			for (list_t* node : list::iter_helper_t{&pl.head}) {
				os << node->data << " ";
			}
			return os;
		}

		list_t& head;
	};

	template<class data_t>
	struct print_data_t {
		friend std::ostream& operator << (std::ostream& os, const print_data_t& pd) {
			if (pd.data != 0) {
				os << pd.data;
			} else {
				os << "head";
			}
			return os;
		}

		data_t data;
	};

	int test_creation_destruction() {
		list_t head{};
		create_list(head, 10);
		std::cout << print_list_t{head} << std::endl;
		destroy_list(head);
		std::cout << "test finished" << std::endl;
		std::cout << std::endl;
		return 0;
	}

	int test_list_split_append() {
		int count = 10;
		list_t head{};
		create_list(head, count);

		std::cout << "initial list: " << print_list_t(head) << std::endl;
		for (int i = -1; i <= count + 1; i++) {
			list_t part1, part2;
			list::split(&head, i, &part1, &part2);
			std::cout << "first_part = " << i << std::endl;
			std::cout << "part1: " << print_list_t{part1} << std::endl;
			std::cout << "part2: " << print_list_t{part2} << std::endl;
			list::append(&head, &part1);
			list::append(&head, &part2);
			std::cout << "reunited: " << print_list_t{head} << std::endl;
			std::cout << std::endl;
		}

		destroy_list(head);
		std::cout << "test finished" << std::endl;
		std::cout << std::endl;
		return 0;
	}

	int test_list_swap_prev_next() {
		int count = 10;
		list_t head{};
		create_list(head, count);

		std::cout << "initial list: " << print_list_t{head} << std::endl;
		std::cout << "testing swap_prev" << std::endl;
		for (int i = 0; i < count; i++) {
			list::swap_prev(&head);
			std::cout << i << ": " << print_list_t{head} << std::endl;
		}

		std::cout << "testing swap_next" << std::endl;
		for (int i = 0; i < count; i++) {
			list::swap_next(&head);
			std::cout << i << ": " << print_list_t{head} << std::endl;
		}

		destroy_list(head);
		std::cout << "test finished" << std::endl;
		std::cout << std::endl;
		return 0;
	}

	int test_list_swap_nodes() {
		constexpr int nodes_count = 8;

		list_t nodes[nodes_count] = {}; // nodes[0] is head

		list::init(&nodes[0]);
		for (int i = 0; i < nodes_count; i++) {
			nodes[i].data = i;
			list::insert_before(&nodes[0], &nodes[i]);
		}

		std::cout << "list: " << print_list_t{nodes[0]} << std::endl;

		struct swap_t {
			int i, j;
		} swaps[] = {{0, 2}, {0, 4}, {0, 6}, {0, 2}, {2, 4}, {4, 6}};

		for (auto [i, j] : swaps) {
			list::swap_nodes(&nodes[i], &nodes[j]);
			std::cout << "swap(" << print_data_t{i} << "," << print_data_t{j} << "): " << print_list_t{nodes[0]} << std::endl;
		}

		std::cout << "test finished" << std::endl;
		std::cout << std::endl;
		return 0;
	}

	int test_swap_heads() {
		int count = 10;
		list_t head1{}, head2{};
		create_list(head1, count);
		create_list(head2, 0);

		std::cout << "head1 before: " << print_list_t{head1} << std::endl;
		std::cout << "head2 before: " << print_list_t{head2} << std::endl;
		list::swap_heads(&head1, &head2);
		std::cout << "head1 after: " << print_list_t{head1} << std::endl;
		std::cout << "head2 after: " << print_list_t{head2} << std::endl;

		destroy_list(head1);
		destroy_list(head2);
		std::cout << "test finished" << std::endl;
		std::cout << std::endl;
		return 0;
	}
}

int main(int argc, char* argv[]) {
	//test_creation_destruction();
	//test_list_split_append();
	//test_list_swap_prev_next();
	//test_list_swap_nodes();
	test_swap_heads();
	return 0;
}