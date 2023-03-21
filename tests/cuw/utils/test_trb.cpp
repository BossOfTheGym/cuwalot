#include <set>
#include <cmath>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <random>
#include <cassert>
#include <sstream>
#include <utility>
#include <numeric>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <type_traits>

#include <cuw/utils/trb.hpp>
#include <cuw/utils/trb_node.hpp>

#include "pool.hpp"

using namespace cuw;

template<class ... args_t>
std::string join(args_t&& ... args) {
	std::ostringstream oss;
	(oss << ... << args);
	return oss.str();
}

using traits_t = cuw::trb::tree_node_packed_traits_t<int>;
using node_t = cuw::trb::tree_node_packed_t<traits_t>;
using color_t = cuw::trb::node_color_t;

struct key_ops_t {
	void set_key(node_t* node, int key) const {
		node->data = key;
	}

	int get_key(node_t* node) const {
		return node->data;
	}

	bool compare(int lhs, int rhs) const {
		return lhs < rhs;
	}
};

inline constexpr int max_nodes = 10'000'000;

// for profiling
#ifdef _MSC_VER
	//#define NOINLINE __declspec(noinline)
	#define NOINLINE
#else
	#define NOINLINE
#endif

class basic_tree_storage_t {
public:
	template<class ... args_t>
	NOINLINE node_t* emplace(args_t&& ... args) {
		return new node_t{std::forward<args_t>(args)...};
	}

	NOINLINE void destroy(node_t* node) {
		delete node;
	}
};

class pooled_tree_storage_t {
public:
	using node_pool_t = pool_storage_t<node_t, max_nodes>;

	pooled_tree_storage_t() {
		pool = std::make_unique<node_pool_t>();
	}

	template<class ... args_t>
	NOINLINE node_t* emplace(args_t&& ... args) {
		return pool->emplace(std::forward<args_t>(args)...);
	}

	NOINLINE void destroy(node_t* node) {
		pool->destroy(node);
	}

private:
	std::unique_ptr<node_pool_t> pool;
};

template<class __storage_t>
class basic_tree_t {
public:
	using storage_t = __storage_t;

	~basic_tree_t() {
		clear();
	}

	void insert(int key) {
		auto alloc_wrapper = [&] (auto&& ... args) { return storage.emplace(std::forward<decltype(args)>(args)...); };

		node_t* node = trb::alloc_node<node_t>(alloc_wrapper, key);
		if (node) {
			root = trb::insert_ub(root, node, key_ops);
			return;
		} throw std::runtime_error("out of memory");
	}

	bool remove(int key) {
		node_t* lb = bst::lower_bound(root, key_ops, key);
		if (lb && !(key < lb->data)) {
			root = trb::remove(root, lb);
			storage.destroy(lb);
			return true;
		} return false;
	}

	bool has(int key) {
		node_t* lb = bst::lower_bound(root, key_ops, key);
		return lb && !(key < lb->data);
	}

	std::ostream& print(std::ostream& os) const {
		bst::traverse_inorder(root, [&] (node_t* node) { os << node->data << " "; });
		return os;
	}

	void clear() {
		bst::traverse_postorder(root, [&] (node_t* node) { storage.destroy(node); });
		root = nullptr;
	}

	bool check_invariant() const {
		return trb::check_rb_invariant(root);
	}

private:
	key_ops_t key_ops{};
	storage_t storage;
	node_t* root{};
};

// lol, pooled_tree_t is slower than simple_tree_t
using simple_tree_t = basic_tree_t<basic_tree_storage_t>;
using pooled_tree_t = basic_tree_t<pooled_tree_storage_t>;
//using tree_t = pooled_tree_t;
using tree_t = simple_tree_t;

std::ostream& operator << (std::ostream& os, const tree_t& tree) {
	return tree.print(os);
}

enum tree_op_type_t : unsigned {
	Insert = 0,
	Remove = 1,
	Has = 2,
	Print = 3,
	CheckAllocations = 4,
	CheckInvariant = 5,
};

struct tree_op_t {
	tree_op_type_t op;
	int key{};
	bool present{};
};

using tree_ops_t = std::vector<tree_op_t>;

std::ostream& operator << (std::ostream& os, const tree_ops_t& ops) {
	for (auto [op, key, present] : ops) {
		switch (op) {
			case Insert: {
				os << "i:" << key << " ";
				break;
			} case Remove: {
				os << "r:" << key << "," << present << " ";
				break;
			} case Has: {
				os << "h:" << key << "," << present << " ";
				break;
			} case Print: {
				os << "p ";
				break;
			} case CheckAllocations: {
				os << "ca ";
				break;
			} case CheckInvariant: {
				os << "ci ";
				break;
			} default: {
				os << "iii ";
				break;
			}
		}
	} return os;
}

int tree_test(int nodes, const tree_ops_t& ops) {
	auto format_err_msg = [] (std::string msg, int key, const tree_t& tree) {
		return join("[", msg, "]", " key: ", key, " tree: ", tree);
	};

	tree_t tree;
	for (auto [op, key, present] : ops) {
		switch (op) {
			case Insert: {
				tree.insert(key);
				break;
			} case Remove: {
				if (tree.remove(key) != present) {
					throw std::runtime_error(format_err_msg("remove", key, tree));
				} break;
			} case Has: {
				if (tree.has(key) != present) {
					throw std::runtime_error(format_err_msg("has", key, tree));
				} break;
			} case Print: {
				tree.print(std::cout);
				break;
			} case CheckInvariant: {
				if (!tree.check_invariant()) {
					throw std::runtime_error("invariant");
				} break;
			} default: {
				throw std::runtime_error("invalid op");
				break;
			}
		}
	} return 0;
}

int tree_test1() {
	std::cout << "test1" << std::endl;

	int nodes = 10;
	tree_ops_t ops;
	for (int i = 0; i < nodes; i++) {
		ops.push_back({Insert, i});
		ops.push_back({CheckInvariant});
		for (int j = 0; j <= i; j++) {
			ops.push_back({Has, j, true});
		} for (int j = i + 1; j < nodes; j++) {
			ops.push_back({Has, j, false});
		}
	} for (int i = 0; i < nodes; i++) {
		ops.push_back({Remove, i, true});
		ops.push_back({CheckInvariant});
		for (int j = 0; j <= i; j++) {
			ops.push_back({Has, j, false});
		} for (int j = i + 1; j < nodes; j++) {
			ops.push_back({Has, j, true});
		}
	}

	tree_test(nodes, ops);

	std::cout << "test1 passed" << std::endl;
	return 0;
}

int tree_test2() {
	std::cout << "test2" << std::endl;

	int nodes = 10;
	tree_ops_t ops;
	for (int i = 0; i < nodes; i++) {
		ops.push_back({Insert, i});
		ops.push_back({CheckInvariant});
		for (int j = 0; j <= i; j++) {
			ops.push_back({Has, j, true});
		} for (int j = i + 1; j < nodes; j++) {
			ops.push_back({Has, j, false});
		}
	} for (int i = nodes - 1; i >= 0; i--) {
		ops.push_back({Remove, i, true});
		ops.push_back({CheckInvariant});
		for (int j = nodes - 1; j >= i; j--) {
			ops.push_back({Has, j, false});
		} for (int j = i - 1; j >= 0; j--) {
			ops.push_back({Has, j, true});
		}
	}

	tree_test(nodes, ops);

	std::cout << "test2 passed" << std::endl;
	return 0;
}

int tree_test3() {
	std::cout << "test3" << std::endl;

	int nodes = 10;
	tree_ops_t ops = {
		{Insert, 0}, {CheckInvariant},
		{Insert, 6}, {CheckInvariant},
		{Insert, 4}, {CheckInvariant},
		{Insert, 3}, {CheckInvariant},
		{Insert, 9}, {CheckInvariant},
		{Insert, 5}, {CheckInvariant},
		{Has, 0, true}, {Has, 6, true}, {Has, 4, true}, {Has, 3, true}, {Has, 9, true}, {Has, 5, true},

		{Remove, 4, true}, {CheckInvariant},
		{Remove, 5, true}, {CheckInvariant},
		{Has, 4, false}, {Has, 5, false},

		{Insert, 1}, {CheckInvariant},
		{Insert, 7}, {CheckInvariant},
		{Insert, 2}, {CheckInvariant},
		{Insert, 8}, {CheckInvariant},
		{Has, 1, true}, {Has, 7, true}, {Has, 2, true}, {Has, 8, true},

		{Remove, 3, true}, {CheckInvariant},
		{Remove, 9, true}, {CheckInvariant},
		{Has, 3, false}, {Has, 9, false},

		{Insert, 4}, {CheckInvariant},
		{Insert, 5}, {CheckInvariant},
		{Has, 4, true}, {Has, 5, true},

		{Remove, 0, true}, {CheckInvariant},
		{Remove, 6, true}, {CheckInvariant},
		{Remove, 2, true}, {CheckInvariant},
		{Remove, 8, true}, {CheckInvariant},
		{Remove, 1, true}, {CheckInvariant},
		{Remove, 4, true}, {CheckInvariant},
		{Remove, 7, true}, {CheckInvariant},
		{Remove, 5, true}, {CheckInvariant},
		{Has, 0, false}, {Has, 6, false}, {Has, 2, false}, {Has, 8, false}, {Has, 1, false}, {Has, 4, false}, {Has, 7, false}, {Has, 5, false},
	};

	tree_test(nodes, ops);

	std::cout << "test3 passed" << std::endl;
	return 0;
}

using seq_t = std::vector<int>;

struct seq_wrapper_t {
	const seq_t& seq;
};

std::ostream& operator << (std::ostream& os, const seq_wrapper_t& obj) {
	for (int i = 1; i < obj.seq.size(); i++) {
		os << obj.seq[i - 1] << " ";
	} if (obj.seq.size() != 0) {
		os << obj.seq.back();
	} return os;
}

int tree_factorial_test() {
	std::cout << "factorial test" << std::endl;

	auto format_err_msg = [] (std::string msg, int key, const tree_t& tree, const seq_t& seq) {
		return join("[", msg, "]", " key: ", key, " tree: ", tree, " seq: ", seq_wrapper_t{seq});
	};

	int nodes = 10;
	tree_t tree;

	seq_t seq(nodes);
	std::iota(seq.begin(), seq.end(), 0);

	bool insert = true;
	do {
		if (insert) {
			for (auto key : seq) {
				tree.insert(key);
				if (!tree.check_invariant()) {
					throw std::runtime_error(format_err_msg("insert,invariant", key, tree, seq));
				}
			} insert = false;
		} else {
			for (auto key : seq) {
				if (!tree.remove(key)) {
					throw std::runtime_error(format_err_msg("remove", key, tree, seq));
				} if (!tree.check_invariant()) {
					throw std::runtime_error(format_err_msg("remove,invariant", key, tree, seq));
				}
			} insert = true;
		}
	} while(std::next_permutation(seq.begin(), seq.end()));

	std::cout << "factorial test passed" << std::endl;
	return 0;
}

int tree_full_factorial_test() {
	std::cout << "full factorial test" << std::endl;

	auto format_err_msg = [] (std::string msg, int key, const seq_t& insert_seq, const seq_t& remove_seq) {
		return join("[", msg, "]", " key: ", key, " ins: ", seq_wrapper_t{insert_seq}, " rem: ", seq_wrapper_t{remove_seq});
	};

	int nodes = 7;
	tree_t tree;
	seq_t insert_seq(nodes);
	seq_t remove_seq(nodes);

	std::iota(insert_seq.begin(), insert_seq.end(), 0);
	std::iota(remove_seq.begin(), remove_seq.end(), 0);
	do {
		do {
			for (auto key : insert_seq) {
				tree.insert(key);
				if (!tree.check_invariant()) {
					throw std::runtime_error(format_err_msg("insert,invariant", key, insert_seq, remove_seq));
				}
			} for (auto key : insert_seq) {
				tree.insert(key);
				if (!tree.check_invariant()) {
					throw std::runtime_error(format_err_msg("insert,invariant(1)", key, insert_seq, remove_seq));
				}
			} for (auto key : remove_seq) {
				if (!tree.remove(key)) {
					throw std::runtime_error(format_err_msg("remove", key, insert_seq, remove_seq));
				} if (!tree.check_invariant()) {
					throw std::runtime_error(format_err_msg("remove,invariant", key, insert_seq, remove_seq));
				}
			} for (auto key : remove_seq) {
				if (!tree.remove(key)) {
					throw std::runtime_error(format_err_msg("remove(1)", key, insert_seq, remove_seq));
				} if (!tree.check_invariant()) {
					throw std::runtime_error(format_err_msg("remove,invariant(1)", key, insert_seq, remove_seq));
				}
			}
		} while (std::next_permutation(remove_seq.begin(), remove_seq.end()));
	} while (std::next_permutation(insert_seq.begin(), insert_seq.end()));

	std::cout << "full factorial test passed" << std::endl;
	return 0;
}

inline constexpr int tests_skip = 2;
inline constexpr int tests_count = 10;
inline constexpr int tests_nodes = max_nodes;

struct test_result_t {
	double min{}; // ms
	double max{}; // ms
	double avg{}; // ms
	double dev{}; // ms
};

template<class test_func_t>
NOINLINE test_result_t __stress_test(test_func_t test_func, int skip = 1, int k = 3) {
	assert(skip > 0 && skip + 1 < k);

	std::vector<long long> measurements;
	for (int i = 0; i < k; i++) {
		measurements.push_back(test_func());
	}

	double total = 0;
	for (int i = skip; i < k; i++) {
		total += measurements[i];
	}

	double avg = total / (k - skip);

	double dev = 0.0;
	for (int i = skip; i < k; i++) {
		dev += (measurements[i] - avg) * (measurements[i] - avg);
	} dev = std::sqrt(dev / (k - skip - 1));

	double min = *std::min_element(measurements.begin() + skip, measurements.end());
	double max = *std::max_element(measurements.begin() + skip, measurements.end());
	return {min, max, avg, dev};
}

template<class test_func_t>
NOINLINE int run_test(test_func_t test_func, std::string name) {
	std::cout << name << std::endl;
	auto [min, max, avg, dev] = __stress_test(test_func, tests_skip, tests_count);
	auto old = std::cout.precision();
	std::cout << name << ": min/max/avg/dev " << std::setprecision(6)
		<< min << "/" << max << "/" << avg << "/" << dev << std::endl
		<< std::setprecision(old);
	return 0;
}

NOINLINE long long __tree_stress_test() {
	int nodes = tests_nodes;
	tree_t tree;		
	
	auto t0 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < nodes; i++) {
		tree.insert(i);
	} for (int i = 0; i < nodes; i++) {
		tree.remove(i);
	} auto t1 = std::chrono::high_resolution_clock::now();
	
	auto dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
	std::cout << "test passsed in " << dt_ms << "ms" << std::endl;
	return dt_ms;
}


int tree_stress_test() {
	return run_test(__tree_stress_test, "tree_stress_test");
}

NOINLINE long long __tree_stress_test1() {
	int nodes = tests_nodes;
	tree_t tree;		
	
	auto t0 = std::chrono::high_resolution_clock::now();
	for (int i = nodes - 1; i >= 0; i--) {
		tree.insert(i);
	} for (int i = 0; i < nodes; i++) {
		tree.remove(i);
	} auto t1 = std::chrono::high_resolution_clock::now();
	
	auto dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
	std::cout << "test passsed in " << dt_ms << "ms" << std::endl;
	return dt_ms;
}

int tree_stress_test1() {
	return run_test(__tree_stress_test1, "tree_stress_test1");
}

NOINLINE long long __stress_test_std_set() {
	int nodes = tests_nodes;
	std::multiset<int> tree;

	auto t0 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < nodes; i++) {
		tree.insert(i);
	} for (int i = 0; i < nodes; i++) {
		tree.erase(i);
	} auto t1 = std::chrono::high_resolution_clock::now();

	auto dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
	std::cout << "test passsed in " << dt_ms << "ms" << std::endl;
	return dt_ms;
}

int stress_test_std_set() {
	return run_test(__stress_test_std_set, "stress_test_std_set");
}

NOINLINE long long __stress_test_std_set1() {
	int nodes = tests_nodes;
	std::multiset<int> tree;

	auto t0 = std::chrono::high_resolution_clock::now();
	for (int i = nodes - 1; i >= 0; i--) {
		tree.insert(i);
	} for (int i = 0; i < nodes; i++) {
		tree.erase(i);
	} auto t1 = std::chrono::high_resolution_clock::now();

	auto dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
	std::cout << "test passsed in " << dt_ms << "ms" << std::endl;
	return dt_ms;
}

int stress_test_std_set1() {
	return run_test(__stress_test_std_set1, "stress_test_std_set1");
}

//inline constexpr int seed = 42;
inline constexpr int seed = 69420;

NOINLINE long long __tree_stress_random() {
	int nodes = max_nodes;

	tree_t tree;
	std::vector<int> keys;
	std::minstd_rand gen(seed);

	keys.reserve(nodes);

	auto t0 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < nodes; i++) {
		tree.insert((int)gen());
	} while (!keys.empty()) {
		auto index = gen() % keys.size();
		std::swap(keys.back(), keys[index]);
		tree.remove(keys.back());
		keys.pop_back();
	} auto t1 = std::chrono::high_resolution_clock::now();

	auto dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
	std::cout << "test passsed in " << dt_ms << "ms" << std::endl;
	return dt_ms;
}

int tree_stress_random() {
	return run_test(__tree_stress_random, "tree_stress_random");
}

NOINLINE long long __std_set_stress_random() {
	int nodes = max_nodes;

	std::multiset<int> tree;
	std::vector<int> keys;
	std::minstd_rand gen(seed);

	keys.reserve(nodes);

	auto t0 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < nodes; i++) {
		tree.insert((int)gen());
	} while (!keys.empty()) {
		auto index = gen() % keys.size();
		std::swap(keys.back(), keys[index]);
		tree.erase(keys.back());
		keys.pop_back();
	} auto t1 = std::chrono::high_resolution_clock::now();

	auto dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
	std::cout << "test passsed in " << dt_ms << "ms" << std::endl;
	return dt_ms;
}

int std_set_stress_random() {
	return run_test(__std_set_stress_random, "std_set_stress_random");
}

int main(int argc, char* argv[]) {
	try {
		if (tree_test1()) {
			return -1;
		} if (tree_test2()) {
			return -1;
		} if (tree_test3()) {
			return -1;
		} /*if (tree_factorial_test()) {
			return -1;
		} if (tree_full_factorial_test()) {
			return -1;
		} */if (tree_stress_test() || tree_stress_test1()) {
			return -1;
		} if (stress_test_std_set() || stress_test_std_set1()) {
			return -1;
		} if (tree_stress_random() || std_set_stress_random()) {
			return -1;
		} return 0;
	} catch (std::runtime_error& err) {
		std::cerr << "testing failed: " << err.what() << std::endl;
	} catch (...) {
		std::cerr << "unknown error" << std::endl;
	} return -1;
}