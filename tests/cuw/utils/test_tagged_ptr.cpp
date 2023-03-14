#include <iostream>

#include <cuw/utils/ptr.hpp>

using namespace cuw;

int main(int argc, char* argv[]) {
	int a = 0;
	tagged_ptr_t<int, 4> ptr = &a;
	ptr.set_data(0xA);
	return 0;
}