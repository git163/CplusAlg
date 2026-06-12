# src/cplus_alg/threading/CMakeLists.txt — threading 子模块
# 源文件物理上位于 src/threading/，此处通过绝对路径引入。

list(APPEND CPLUS_ALG_LIB_SOURCES
    ${CMAKE_SOURCE_DIR}/src/threading/PyThreadPool.cpp
)

set(CPLUS_ALG_LIB_SOURCES ${CPLUS_ALG_LIB_SOURCES} PARENT_SCOPE)
