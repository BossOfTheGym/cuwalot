set(common_src dummy_alloc.hpp utils.hpp)

add_executable(test_alloc_descr test_alloc_descr.cpp ${common_src})
target_link_libraries(test_alloc_descr cuw)

add_executable(test_mem_api test_mem_api.cpp ${common_src})
target_link_libraries(test_mem_api cuw)

add_executable(test_sys_alloc test_sys_alloc.cpp ${common_src})
target_link_libraries(test_sys_alloc cuw)

add_executable(test_cached_alloc test_cached_alloc.cpp ${common_src})
target_link_libraries(test_cached_alloc cuw)

add_executable(test_block_pool test_block_pool.cpp ${common_src})
target_link_libraries(test_block_pool cuw)

add_executable(test_page_alloc test_page_alloc.cpp ${common_src})
target_link_libraries(test_page_alloc cuw)

add_executable(test_alloc_wrappers test_alloc_wrappers.cpp ${common_src})
target_link_libraries(test_alloc_wrappers cuw)

add_executable(test_pool_alloc test_pool_alloc.cpp ${common_src})
target_link_libraries(test_pool_alloc cuw)