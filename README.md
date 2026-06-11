# CplusAlg

一行项目描述。

## 环境要求

- CMake ≥ 3.20
- 支持 C++17 的编译器（GCC ≥ 7、Clang ≥ 5、MSVC ≥ 19.14）
- 可选但推荐：GDB 或 LLDB 用于调试；VSCode + 推荐扩展可获得最佳体验。

## 构建

```bash
cmake -S . -B build
cmake --build build -j
```

默认构建类型为 `RelWithDebInfo`——已优化但保留调试信息，崩溃时仍能输出可用的堆栈跟踪。如需真正的 Release 构建：

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
```

## 运行

```bash
./build/CplusAlg
```

## 测试

```bash
ctest --test-dir build --output-on-failure
```

测试文件放在 `tests/` 目录，命名格式为 `Test*.cpp`——`tests/CMakeLists.txt` 会通过 `gtest_discover_tests` 自动发现，新增测试只需创建 `tests/TestMyModule.cpp`。

## 调试

在 VSCode 中打开文件夹（`code .`）。项目已预置启动配置（`.vscode/launch.json`）：

- **(gdb) Launch CplusAlg** / **(lldb) Launch CplusAlg**——构建并调试主程序。Linux 选 gdb，macOS 选 lldb。
- **(gdb) Run unit_tests** / **(lldb) Run unit_tests**——构建并直接调试测试二进制文件。在任意 `Test*.cpp` 中设置断点，按 F5 即可。

预启动任务为 `cmake build`，因此开始调试时二进制文件始终是最新的。

如果不使用 VSCode，可从命令行附加：

```bash
gdb --args ./build/CplusAlg     # Linux
lldb ./build/CplusAlg           # macOS
```

## 项目结构

- `docs/` — 设计文档与计划（使用 `docs/plan-template.md`）
- `src/` — 实现代码（`.cpp`）
- `src/include/` — 公共头文件（Google C++ 风格）
- `tests/` — GTest 单元测试（`Test*.cpp`）

## 规范

见项目根目录的 `CLAUDE.md`。
