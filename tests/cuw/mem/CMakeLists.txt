add_executable(test_mem_api test_mem_api.cpp)
target_link_libraries(test_mem_api cuw)

add_executable(test_sys_alloc test_sys_alloc.cpp)
target_link_libraries(test_sys_alloc cuw)

add_executable(test_cached_alloc test_cached_alloc.cpp)
target_link_libraries(test_cached_alloc cuw)

add_executable(test_block_pool test_block_pool.cpp)
target_link_libraries(test_block_pool cuw)

add_executable(test_page_alloc test_page_alloc.cpp)
target_link_libraries(test_page_alloc cuw)