set(cuw_utils_headers
    utils/bst.hpp
    utils/list.hpp
    utils/ptr.hpp
    utils/trb_node.hpp
    utils/trb.hpp
    utils/utils.hpp)

set(cuw_mem_sources
    mem/cuwalot.cpp
    mem/platform/${platform}/mem_api.cpp)

set(cuw_mem_headers
    mem/alloc_descr.hpp
    mem/alloc_entries.hpp
    mem/alloc_tag.hpp
    mem/alloc_traits.hpp
    mem/alloc_wrappers.hpp
    mem/block_pool.hpp
    mem/cached_alloc.hpp
    mem/core.hpp
    mem/list_cache.hpp
    mem/mem_api.hpp
    mem/page_alloc.hpp
    mem/pool_alloc.hpp
    mem/sys_alloc.hpp
    mem/config.hpp
    mem/cuwalot.hpp
)

add_library(cuw ${CUWALOT_BUILD_TYPE})

generate_export_header(cuw EXPORT_FILE_NAME ${CUWALOT_BUILD_DIR}/src/cuw/export.hpp)

target_sources(cuw
    PRIVATE
        ${cuw_mem_sources}
        ${cuw_mem_headers}
        ${cuw_utils_headers}
        ${CUWALOT_BUILD_DIR}/src/cuw/export.hpp)

target_include_directories(cuw
    PUBLIC 
        $<BUILD_INTERFACE:${CUWALOT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CUWALOT_BUILD_DIR}/src>
        $<INSTALL_INTERFACE:${CUWALOT_INSTALL_INCLUDEDIR}>)

configure_package_config_file(${CUWALOT_SOURCE_DIR}/src/cuw/Config.cmake.in ${CUWALOT_BINARY_DIR}/cuw/cuwConfig.cmake
    INSTALL_DESTINATION ${CUWALOT_CONFIG_DIR}
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO)

write_basic_package_version_file(${CUWALOT_BINARY_DIR}/cuw/cuwConfigVersion.cmake
    VERSION ${CUWALOT_VERSION}
    COMPATIBILITY AnyNewerVersion)

install(FILES ${cuw_utils_headers} DESTINATION ${CUWALOT_INSTALL_INCLUDEDIR}/cuw/utils)
install(FILES ${cuw_mem_headers} DESTINATION ${CUWALOT_INSTALL_INCLUDEDIR}/cuw/mem)
install(FILES ${CUWALOT_BUILD_DIR}/src/cuw/export.hpp DESTINATION ${CUWALOT_INSTALL_INCLUDEDIR}/cuw)

install(FILES
    ${CUWALOT_BINARY_DIR}/cuw/cuwConfig.cmake 
    ${CUWALOT_BINARY_DIR}/cuw/cuwConfigVersion.cmake
    DESTINATION ${CUWALOT_CONFIG_DIR})

install(TARGETS cuw
    EXPORT cuwTargets
    RUNTIME DESTINATION "${CUWALOT_INSTALL_BINDIR}"
    ARCHIVE DESTINATION "${CUWALOT_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CUWALOT_INSTALL_LIBDIR}")

install(EXPORT cuwTargets
    FILE cuwTargets.cmake
    NAMESPACE cuwalot::
    DESTINATION ${CUWALOT_CONFIG_DIR})
