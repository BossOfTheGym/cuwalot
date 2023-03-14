#include <cstring>
#include <iostream>

#include <cuw/mem/alloc_traits.hpp>
#include <cuw/mem/sys_alloc.hpp>

using namespace cuw;

using traits_t = mem::empty_traits_t;
using allocator_t = mem::sys_alloc_t<traits_t>;

namespace {
	int test_sys_alloc() {
		allocator_t alloc;

		std::size_t alloc_size0 = 12345678;
		std::size_t alloc_size1 = 123456789;

		void* ptr = alloc.allocate(alloc_size0);
		memset(ptr, 0xFF, alloc_size0);
		ptr = alloc.reallocate(ptr, alloc_size0, alloc_size1);
		memset(ptr, 0xFF, alloc_size1);
		ptr = alloc.reallocate(ptr, alloc_size1, alloc_size0);
		memset(ptr, 0xFF, alloc_size0);
		alloc.deallocate(ptr, alloc_size0);
		std::cout << ":D" << std::endl;
		return 0;
	}
}

int main(int argc, char* argv[]) {
	return test_sys_alloc();
}