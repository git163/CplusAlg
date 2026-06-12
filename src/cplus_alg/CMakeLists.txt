set(CPLUS_ALG_LIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/alg_interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shm_buffer.cpp
)

add_subdirectory(backend)
add_subdirectory(transport)
add_subdirectory(python)

add_library(cplus_alg_lib STATIC ${CPLUS_ALG_LIB_SOURCES})

target_include_directories(cplus_alg_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${NLOHMANN_JSON_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
)

target_compile_definitions(cplus_alg_lib PUBLIC CPLUS_ALG_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

if(TARGET pybind11::embed)
    target_link_libraries(cplus_alg_lib PUBLIC pybind11::embed)
endif()
