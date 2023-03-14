set(cuw_sources ${cuw_sources}
    utils/bst.hpp
    utils/list.hpp
    utils/ptr.hpp
    utils/trb_node.hpp
    utils/trb.hpp
    utils/utils.hpp)

set(cuw_sources ${cuw_sources}
    mem/alloc_tag.hpp
    mem/mem_api.hpp
    mem/platform/${platform}/mem_api.cpp
    mem/block_pool.hpp
    mem/cached_alloc.hpp
    mem/core.hpp
    mem/list_cache.hpp
    mem/page_alloc.hpp
    mem/sys_alloc.hpp
    mem/alloc_traits.hpp
)

add_library(cuw SHARED ${cuw_sources})
target_include_directories(cuw PUBLIC "../")