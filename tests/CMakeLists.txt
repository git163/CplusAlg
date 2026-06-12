# tests/CMakeLists.txt — 单元测试入口
# 使用 third_party/googletest，不修改第三方源码。

# 避免安装 gtest 到系统
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
# Windows 下强制使用 shared CRT，避免链接错误
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/googletest ${CMAKE_BINARY_DIR}/third_party/googletest)

include(GoogleTest)

# 显式列出测试文件，新增测试时需要手动维护
add_executable(unit_tests
    alg/TestAlgInterface.cpp
    alg/TestBackend.cpp
    alg/TestTemplateMatch.cpp
    interpreter/TestGilManager.cpp
    interpreter/TestPyInterpreter.cpp
    python/TestPythonBackend.cpp
    python/TestTypeConverter.cpp
    threading/TestPyThreadPool.cpp
    threading/TestPyProducerConsumer.cpp
    threading/TestPyBatchConsumer.cpp
    threading/TestParallelExecutor.cpp
)

target_link_libraries(unit_tests PRIVATE GTest::gtest_main cplus_alg_lib)
target_include_directories(unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/nlohmann_json
)

# 让每个 TEST() 自动注册为 CTest 用例，并设置运行超时防止死锁挂起
gtest_discover_tests(unit_tests PROPERTIES TIMEOUT 60)
