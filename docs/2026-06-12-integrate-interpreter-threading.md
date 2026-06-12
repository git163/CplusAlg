# 集成 src/interpreter 与 src/threading 模块并完成冒烟测试

- 日期: 2026-06-12
- 作者: Claude Code
- 状态: 已批准
- 关联: 用户请求 `/using-superpowers` 引入两个模块并做冒烟测试

## 背景

项目新增了两个 C++ 模块：

- `src/interpreter/`：管理内嵌 Python 解释器生命周期与 GIL 的工具（`GilManager.h`、`PyInterpreter.h/.cpp`）。
- `src/threading/`：面向 Python 多线程调度的并发组件（`ParallelExecutor.h`、`PyProducerConsumer.h`、`PyBatchConsumer.h`、`PyThreadPool.h/.cpp`）。

这两个模块当前存在以下问题：

1. **未被构建系统收录**：根 `CMakeLists.txt` 只加了 `src/cplus_alg/`，`src/interpreter/` 和 `src/threading/` 没有出现在任何 `CMakeLists.txt` 中。
2. **`PyInterpreter.cpp` 编译失败**：它 `#include "Utils.h"` 和 `#include "FileSystemUtils.h"`，但这两个文件不存在。
3. **两套 Python 解释器管理并存**：现有 `src/cplus_alg/python/python_backend.cpp` 已经自行管理了一个 `py::scoped_interpreter` 单例。如果 `PyInterpreter` 同时初始化，会触发 pybind11 "interpreter already running" 的致命错误。
4. **缺少测试**：`src/interpreter/` 和 `src/threading/` 没有任何单元/冒烟测试，且线程池测试若设计不当容易产生死锁或长时间挂起。

## 目标

1. 将 `src/interpreter/` 与 `src/threading/` 正式纳入 `cplus_alg_lib` 的构建流程。
2. 修复 `PyInterpreter.cpp` 的缺失依赖，并用 `std::filesystem` 重构路径处理。
3. 让 `PyInterpreter` 成为进程内唯一的 Python 解释器管理者；`python_backend` 不再自行创建解释器，而是复用 `PyInterpreter`。
4. 为两个模块添加冒烟/单元测试，覆盖核心功能，并加入防死锁、防超时的测试保护。
5. 公共头文件保持私有，不暴露到 `src/include/`。

## 非目标

- 不改动 `alg/` 目录下的 Python 算法实现。
- 不引入新的第三方依赖。
- 不调整 `cplus_alg_lib` 以外的可执行目标（`main.cpp`）的业务逻辑。

## 方案

### 目录与构建组织

保持 `src/interpreter/` 和 `src/threading/` 的物理位置不变。在 `src/cplus_alg/CMakeLists.txt` 中直接把它们当作子模块加入，复用已有的 `PARENT_SCOPE` 源文件传播模式：

```text
src/
├── cplus_alg/
│   ├── CMakeLists.txt          # 增加 add_subdirectory(interpreter)、add_subdirectory(threading)
│   ├── interpreter/CMakeLists.txt
│   ├── threading/CMakeLists.txt
│   └── ...
├── interpreter/                # 保持原地
│   ├── GilManager.h
│   ├── PyInterpreter.h
│   └── PyInterpreter.cpp
├── threading/                  # 保持原地
│   ├── ParallelExecutor.h
│   ├── PyBatchConsumer.h
│   ├── PyProducerConsumer.h
│   ├── PyThreadPool.h
│   └── PyThreadPool.cpp
```

### 修复 PyInterpreter.cpp

1. 删除 `#include "Utils.h"` 和 `#include "FileSystemUtils.h"`。
2. 新增 `#include <filesystem>`。
3. 实现一个小的内部辅助函数 `get_executable_path()`（平台相关，Linux/macOS 用 `/proc/self/exe` 或 `_NSGetExecutablePath`），或者直接复用 `python_backend.cpp` 中通过 `sys.argv[0]` 推导路径的思路。
4. `SetupSysPath()` 改用 `std::filesystem::path` 的 `parent_path()`、`operator/`、`exists()` 完成路径拼接与校验。
5. 保留 `PyInterpreter` 的线程安全初始化、GIL 预热和显式 `Finalize()` 能力。

### 统一解释器管理

当前 `python_backend.cpp` 内部有一个 `python_runtime` 单例，它自己持有 `py::scoped_interpreter`。为了避免两套解释器冲突，需要：

1. 让 `python_backend.cpp` 在首次调用时通过 `PyInterpreter::Instance().Initialize()` 启动解释器。
2. 删除 `python_backend.cpp` 中 `python_runtime::ensure_initialized()` 里创建 `py::scoped_interpreter` 的逻辑。
3. `python_runtime` 不再负责解释器构造/析构，只负责加载 `alg.core.registry` 和缓存 `py::object` 句柄。
4. `python_backend::available()` 通过 `PyInterpreter::Instance().IsInitialized()` 与能否成功加载 `alg.core.registry` 共同判断。
5. 保留 `python_runtime` 的堆分配与“不主动 delete”策略，以维持 SIGTRAP 规避效果；解释器关闭统一由 `PyInterpreter::Finalize()` 或进程退出处理。

### GIL 工具对齐

`src/threading/` 已经包含 `GilManager.h` 并使用了 `GilScopedAcquire`/`GilScopedRelease`。统一解释器后：

- 工作线程通过 `GilScopedAcquire` 获取 GIL。
- 主线程在调用 `PyInterpreter::Initialize()` 后默认持有 GIL，需要显式 `GilScopedRelease` 才能启动并行任务。
- `GilScopedRelease` 内部使用 `PyEval_SaveThread` / `PyEval_RestoreThread`，与 `PyInterpreter` 的 `py::initialize_interpreter()` 兼容。

### 测试设计

新增测试目录 `tests/interpreter/` 和 `tests/threading/`，测试文件命名遵循项目规范：

- `tests/interpreter/TestPyInterpreter.cpp`
- `tests/threading/TestPyThreadPool.cpp`
- `tests/threading/TestPyProducerConsumer.cpp`
- `tests/threading/TestParallelExecutor.cpp`
- （可选）`tests/threading/TestPyBatchConsumer.cpp`

`tests/CMakeLists.txt` 更新文件列表并加入新目录。

#### 防死锁 / 防超时机制

1. **CTest 级别超时**：在 `tests/CMakeLists.txt` 中对所有发现的测试设置 `TIMEOUT` 属性（例如 30 秒），确保任何死锁都能在 CI 中被强制终止。
2. **测试内部限时等待**：
   - 线程池提交任务后，用 `std::future::wait_for(...)` 断言结果，而不是 `.get()` 无限阻塞。
   - `WaitAll()` 的测试用限时断言包装，如 `wait_for(std::chrono::seconds(5))`。
3. **小任务、小线程数**：线程池测试使用 1~4 个线程，任务只是简单的整数累加或字符串拼接，避免引入真实 Python 调用或 I/O。
4. **RAII 清理**：每个测试用 `Shutdown()` 或让对象析构自动 join，避免测试间泄漏工作线程。
5. **GIL 安全**：线程池测试中的任务不实际调用 Python（避免解释器初始化问题），只验证线程调度、队列、future 语义；需要 Python 的测试放到 `TestPyInterpreter.cpp` 中单独管理解释器生命周期。

#### 测试覆盖

- `TestPyInterpreter.cpp`：
  - 初始化幂等性（多次 `Initialize()` 只生效一次）。
  - `IsInitialized()` 在初始化前后状态正确。
  - 添加额外 `sys.path` 后可通过 Python 验证。
  - `Finalize()` 后状态回到未初始化。
  - 解释器生命周期与 `python_backend::available()` 联动（解释器启动后后端可用）。
- `TestPyThreadPool.cpp`：
  - 提交单个任务并获取结果。
  - 提交多个任务，验证全部完成。
  - 队列满时阻塞行为（使用有界队列）。
  - `WaitAll()` 等待所有待处理任务。
  - `Shutdown()` 后 `Submit` 返回 `std::nullopt`。
  - 禁止裸指针和 `std::ref` 的编译期检查（可通过单独的 `.cpp` 测试文件或 `static_assert` 触发编译失败的方式验证）。
- `TestPyProducerConsumer.cpp`：
  - 单消费者处理若干任务，验证 `GetProcessedCount()`。
  - `ProduceBatch()` 批量生产。
  - `TryProduce()` 超时行为。
  - `WaitAll()` 等待所有任务完成。
  - 任务抛异常时不崩溃，且 `GetErrorCount()` 增加。
- `TestParallelExecutor.cpp`：
  - `RunInParallel` 在多个线程中并行执行计数器累加。
  - `RunInParallelIterated` 迭代执行并验证总次数。
  - 不使用真实 Python 调用，避免 GIL 与解释器初始化复杂度。

### 头文件可见性

按用户选择，不将 `src/interpreter/` 和 `src/threading/` 的头文件复制或移动到 `src/include/`。它们作为 `cplus_alg_lib` 的私有头文件使用。`cplus_alg_lib` 的 `target_include_directories` 已经包含 `${CMAKE_CURRENT_SOURCE_DIR}/..`（即 `src/` 根目录），因此内部源文件和测试可以用 `"interpreter/PyInterpreter.h"`、 `"threading/PyThreadPool.h"` 的形式包含。

## 影响范围

| 文件/目录 | 变更内容 |
|---|---|
| `src/cplus_alg/CMakeLists.txt` | 增加 `add_subdirectory(interpreter)`、`add_subdirectory(threading)` |
| `src/cplus_alg/interpreter/CMakeLists.txt` | 新增，将 `src/interpreter/PyInterpreter.cpp` 加入 `CPLUS_ALG_LIB_SOURCES` |
| `src/cplus_alg/threading/CMakeLists.txt` | 新增，将 `src/threading/PyThreadPool.cpp` 加入 `CPLUS_ALG_LIB_SOURCES` |
| `src/interpreter/PyInterpreter.cpp` | 删除缺失 include，改用 `std::filesystem` |
| `src/cplus_alg/python/python_backend.cpp` | 复用 `PyInterpreter` 管理解释器生命周期 |
| `tests/CMakeLists.txt` | 新增测试文件、增加 CTest 超时属性 |
| `tests/interpreter/TestPyInterpreter.cpp` | 新增 |
| `tests/threading/TestPyThreadPool.cpp` | 新增 |
| `tests/threading/TestPyProducerConsumer.cpp` | 新增 |
| `tests/threading/TestParallelExecutor.cpp` | 新增 |

## 风险与对策

| 风险 | 对策 |
|---|---|
| 统一解释器管理时破坏已有的 SIGTRAP 修复 | `python_runtime` 保持堆分配且不 delete；解释器关闭统一走 `PyInterpreter::Finalize()` 或进程退出；修改后跑完整测试并观察退出阶段 |
| `PyInterpreter::Finalize()` 在静态析构阶段被调用触发 SIGTRAP | 不在 `~PyInterpreter()` 中调用 `Finalize()`，保持现有设计；主程序或测试显式调用 `Finalize()` |
| 线程池测试死锁导致 CI 卡死 | CTest 设置 30 秒超时；测试内部用 `wait_for` 替代无限阻塞；任务简单且短 |
| `SetupSysPath()` 重构后找不到 `alg` 包 | 保留 `python_backend.cpp` 的 `add_alg_path()` 作为 Python 后端的 path 发现逻辑；`PyInterpreter` 只负责启动解释器和追加用户路径 |
| `PyInterpreter` 与 `python_backend` 循环依赖 | `cplus_alg_lib` 内部先编译 `interpreter` 源文件；`python` 子目录随后编译并链接同一库目标，不存在循环链接 |

## 实施步骤

- [ ] 1. 创建 `src/cplus_alg/interpreter/CMakeLists.txt`，将 `src/interpreter/PyInterpreter.cpp` 加入 `CPLUS_ALG_LIB_SOURCES`。
- [ ] 2. 创建 `src/cplus_alg/threading/CMakeLists.txt`，将 `src/threading/PyThreadPool.cpp` 加入 `CPLUS_ALG_LIB_SOURCES`。
- [ ] 3. 修改 `src/cplus_alg/CMakeLists.txt`，增加 `add_subdirectory(interpreter)` 和 `add_subdirectory(threading)`。
- [ ] 4. 重构 `src/interpreter/PyInterpreter.cpp`：删除 `Utils.h`/`FileSystemUtils.h`，改用 `std::filesystem` 实现 `SetupSysPath()`。
- [ ] 5. 修改 `src/cplus_alg/python/python_backend.cpp`：复用 `PyInterpreter` 初始化解释器，移除 `py::scoped_interpreter` 的局部管理。
- [ ] 6. 配置 CMake 并编译，确保 `cplus_alg_lib` 与主目标能正常构建。
- [ ] 7. 新增 `tests/interpreter/TestPyInterpreter.cpp`。
- [ ] 8. 新增 `tests/threading/TestPyThreadPool.cpp`。
- [ ] 9. 新增 `tests/threading/TestPyProducerConsumer.cpp`。
- [ ] 10. 新增 `tests/threading/TestParallelExecutor.cpp`。
- [ ] 11. 更新 `tests/CMakeLists.txt`：加入新测试文件并设置 `TIMEOUT` 属性。
- [ ] 12. 运行 `ctest --output-on-failure`，修复失败用例。
- [ ] 13. 将本计划保存到 `docs/2026-06-12-integrate-interpreter-threading.md`。

## 测试计划

- **单元测试**：`TestPyInterpreter`、`TestPyThreadPool`、`TestPyProducerConsumer`、`TestParallelExecutor`。
- **冒烟测试**：运行 `ctest --output-on-failure`，确认核心用例通过；由于线程池测试可能死锁，依赖 CTest 超时兜底。
- **回归测试**：完整跑既有 `tests/alg/*` 测试，确保 `python_backend` 的解释器复用没有破坏模板匹配、curve_fit、echo 等功能。
- **可选强化**：在 CI 中增加 `-DENABLE_TSAN=ON` 构建跑线程相关测试，检测数据竞争。

## 验证步骤

```bash
# 1. 配置（mac/linux 通用）
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo

# 2. 编译
cmake --build build -j

# 3. 运行全部测试（CTest 超时兜底）
ctest --test-dir build --output-on-failure

# 4. 单独运行新增测试
./build/tests/unit_tests --gtest_filter='PyInterpreter.*:PyThreadPool.*:PyProducerConsumer.*:ParallelExecutor.*'
```

## 引用

- 项目规范：`CLAUDE.md`（C++17、underscore_style、中文注释文档、英文日志、RelWithDebInfo、测试目录结构）。
- 现有模块：`src/cplus_alg/python/python_backend.cpp`（解释器生命周期与 path 发现）。
- 待集成模块：`src/interpreter/`、`src/threading/`。
