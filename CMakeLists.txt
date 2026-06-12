cmake_minimum_required(VERSION 3.20)
project(CplusAlg LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 默认使用 RelWithDebInfo：优化 + 保留调试符号
# - 性能接近 Release
# - 崩溃时仍能用 gdb/lldb 拿到符号和行号
# - 真正发布时单独切到 Release 即可（-DCMAKE_BUILD_TYPE=Release）
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# 调试信息：所有 build type 都保留 -g，便于事后排查
add_compile_options(-g)

# 第三方库集成（不修改 third_party/ 源码）
# pybind11：按需集成；若未找到 Python3 开发环境则跳过
find_package(Python COMPONENTS Interpreter Development)
if(Python_FOUND)
    add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/pybind11 ${CMAKE_BINARY_DIR}/third_party/pybind11)
else()
    message(WARNING "未找到 Python 开发环境，跳过 pybind11 集成")
endif()

# nlohmann/json：单头文件库，直接加 include 路径
set(NLOHMANN_JSON_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/nlohmann_json)

# OpenCV：cv::Mat 适配器需要，未找到时跳过
find_package(OpenCV QUIET)
if(OpenCV_FOUND)
    message(STATUS "找到 OpenCV ${OpenCV_VERSION}")
else()
    message(WARNING "未找到 OpenCV，cv::Mat 适配器将被跳过")
endif()

# 源码与头文件
# 注意：file(GLOB_RECURSE) 是 CMake 反模式——CMake 不会主动感知新增的 .cpp。
# 缓解：CONFIGURE_DEPENDS 会在 build 时触发 cmake 重跑；如 build 已存在，
#      新增 .cpp 后请先 `cmake --build build --target rebuild_cache`。
# 项目成长后推荐：每个模块一个 CMakeLists.txt，用 add_subdirectory() 替代 GLOB。
file(GLOB_RECURSE CPLUS_ALG_SOURCES CONFIGURE_DEPENDS "src/cplus_alg/*.cpp" "src/cplus_alg/*.cc")
file(GLOB_RECURSE PROJECT_HEADERS CONFIGURE_DEPENDS "src/include/*.h" "src/include/*.hpp")

# 核心库（供主程序和测试共享）
add_library(cplus_alg_lib STATIC ${CPLUS_ALG_SOURCES})
target_include_directories(cplus_alg_lib PUBLIC src src/include ${NLOHMANN_JSON_INCLUDE_DIR})
target_compile_definitions(cplus_alg_lib PUBLIC CPLUS_ALG_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# 若 pybind11 可用，链接 embed 目标以支持内嵌 Python 解释器
if(TARGET pybind11::embed)
    target_link_libraries(cplus_alg_lib PUBLIC pybind11::embed)
endif()

# 若 OpenCV 可用，链接并启用适配器
if(OpenCV_FOUND)
    target_link_libraries(cplus_alg_lib PUBLIC ${OpenCV_LIBS})
    target_include_directories(cplus_alg_lib PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_compile_definitions(cplus_alg_lib PUBLIC CPLUS_ALG_HAS_OPENCV=1)
endif()

# 主目标
file(GLOB_RECURSE MAIN_SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.cc")
# 排除已加入库的 src/cplus_alg/*
list(FILTER MAIN_SOURCES EXCLUDE REGEX "src/cplus_alg/.*")
add_executable(${PROJECT_NAME} ${MAIN_SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE cplus_alg_lib)

# 启用测试
option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
