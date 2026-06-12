# tests/CMakeLists.txt — 单元测试入口
# 使用 third_party/googletest，不修改第三方源码。
# 随项目成长，推荐拆成 tests/<模块>/CMakeLists.txt 各模块独立。

# 避免安装 gtest 到系统
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
# Windows 下强制使用 shared CRT，避免链接错误
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/googletest ${CMAKE_BINARY_DIR}/third_party/googletest)

include(GoogleTest)

# 自动发现 tests/ 下所有 Test*.cpp（含子目录）
# CONFIGURE_DEPENDS：新增测试文件后下次 build 自动重新 configure
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "Test*.cpp")

add_executable(unit_tests ${TEST_SOURCES})
target_link_libraries(unit_tests PRIVATE GTest::gtest_main cplus_alg_lib)
target_include_directories(unit_tests PRIVATE
    ../src/include
    ${CMAKE_SOURCE_DIR}/third_party/nlohmann_json
)

# 让每个 TEST() 自动注册为 CTest 用例
gtest_discover_tests(unit_tests)
