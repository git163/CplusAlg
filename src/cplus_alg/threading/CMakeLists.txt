# src/cplus_alg/threading/CMakeLists.txt — threading 子模块
# 源文件物理上位于 src/threading/，此处通过绝对路径引入。

list(APPEND CPLUS_ALG_LIB_SOURCES
    ${CMAKE_SOURCE_DIR}/src/threading/PyThreadPool.cpp
)

set(CPLUS_ALG_LIB_SOURCES ${CPLUS_ALG_LIB_SOURCES} PARENT_SCOPE)

# 新增：threading_perf 可执行文件（手动跑，不接入 ctest）
add_executable(threading_perf
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_main.cpp
)

target_link_libraries(threading_perf PRIVATE
    cplus_alg_lib
    GTest::gtest
)

target_include_directories(threading_perf PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_features(threading_perf PRIVATE cxx_std_17)
