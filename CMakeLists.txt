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
    # 校验 Python 环境包含运行时必需的包
    execute_process(
        COMMAND ${Python_EXECUTABLE} -c "import numpy; numpy.dtype"
        RESULT_VARIABLE NUMPY_IMPORT_RESULT
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT NUMPY_IMPORT_RESULT EQUAL 0)
        message(FATAL_ERROR "找到的 Python ${Python_EXECUTABLE} 未安装可用的 numpy，请运行: ${Python_EXECUTABLE} -m pip install numpy")
    endif()

    execute_process(
        COMMAND ${Python_EXECUTABLE} -c "import cv2"
        RESULT_VARIABLE CV2_IMPORT_RESULT
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT CV2_IMPORT_RESULT EQUAL 0)
        message(WARNING "找到的 Python ${Python_EXECUTABLE} 未安装 cv2，模板匹配等算法将无法运行")
    endif()

    add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/pybind11 ${CMAKE_BINARY_DIR}/third_party/pybind11)
else()
    message(WARNING "未找到 Python 开发环境，跳过 pybind11 集成")
endif()

# nlohmann/json：单头文件库，直接加 include 路径
set(NLOHMANN_JSON_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/nlohmann_json)

# spdlog：头文件优先，直接加 include 路径
set(SPDLOG_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/spdlog/include)

# 核心库：按模块拆分到 src/cplus_alg/<模块>/CMakeLists.txt
add_subdirectory(src/cplus_alg)

# 主目标
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE cplus_alg_lib)

# 启用测试
option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
