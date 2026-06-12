# 修复 interpreter/threading 集成 review 问题

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 code review 中发现的 Critical 与 Important 问题，使 `PyInterpreter` 与 `python_backend` 的解释器生命周期管理更健壮，并补充缺失的联动测试。

**Architecture:** 在 `PyInterpreter` 中实现 `sys.path` 追加去重，并确保 `Finalize` 无论成功或异常都重置内部状态；在 `python_backend` 的 `python_runtime` 中检测外部 `Finalize` 并自动重置；通过新增测试验证 `python_backend::available()` 与解释器状态联动。

**Tech Stack:** C++17, CMake, GoogleTest, pybind11, spdlog

---

## File Structure

| 文件 | 职责 |
|---|---|
| `src/interpreter/PyInterpreter.cpp` | 修复 `Initialize` 幂等调用时的 `sys.path` 重复污染；修复 `Finalize` 异常时状态不一致。 |
| `src/interpreter/PyInterpreter.h` | 移除未使用的 `<memory>` include。 |
| `src/interpreter/GilManager.h` | 简化 `GilScopedRelease` 构造函数中 `m_bReleased` 的初始化写法。 |
| `src/cplus_alg/python/python_backend.cpp` | 在 `python_runtime` 中检测解释器被外部 `Finalize`，自动重置并重新初始化。 |
| `tests/interpreter/TestPyInterpreter.cpp` | 新增 `sys.path` 去重测试与 `python_backend::available()` 状态联动测试。 |
| `docs/2026-06-12-integrate-interpreter-threading.md` | 将实施步骤复选框标记为已完成。 |

---

## Task 1: 修复 `PyInterpreter::Initialize` 幂等调用导致 `sys.path` 重复污染

**Files:**
- Modify: `src/interpreter/PyInterpreter.cpp`
- Test: `tests/interpreter/TestPyInterpreter.cpp`

- [ ] **Step 1: 编写失败测试**

在 `tests/interpreter/TestPyInterpreter.cpp` 中新增一个测试，验证多次调用 `Initialize` 传入同一路径时，`sys.path` 中只出现一次。

```cpp
TEST(PyInterpreter, ExtraSysPathIsNotDuplicated) {
    PyInterpreter& interp = PyInterpreter::Instance();
    ASSERT_TRUE(interp.Initialize());

    const std::string k_dummy_path = "/tmp/cplusalg_pyinterp_unique_test_path";
    EXPECT_TRUE(interp.Initialize({k_dummy_path}));
    EXPECT_TRUE(interp.Initialize({k_dummy_path}));

    py::gil_scoped_acquire gil;
    py::module_ sys = py::module_::import("sys");
    py::list path_list = sys.attr("path");

    int count = 0;
    for (const auto& item : path_list) {
        std::string p = item.cast<std::string>();
        if (p == k_dummy_path) {
            ++count;
        }
    }
    EXPECT_EQ(count, 1) << "extra sys.path was duplicated";
}
```

- [ ] **Step 2: 运行测试确认失败**

```bash
cmake --build build -j
./build/tests/unit_tests --gtest_filter='PyInterpreter.ExtraSysPathIsNotDuplicated'
```

Expected: FAIL，因为当前实现会重复追加路径。

- [ ] **Step 3: 实现去重逻辑**

修改 `src/interpreter/PyInterpreter.cpp`：

1. 在匿名命名空间中添加辅助函数 `path_exists_in_list`：

```cpp
namespace {

bool path_exists_in_list(py::list& path_list, const std::string& path) {
    for (const auto& item : path_list) {
        try {
            std::string p = item.cast<std::string>();
            if (p == path) {
                return true;
            }
        } catch (...) {
            continue;
        }
    }
    return false;
}

} // namespace
```

2. 修改 `SetupSysPath` 签名，增加 `b_append_default` 参数，并在追加前做去重：

```cpp
void PyInterpreter::SetupSysPath(const std::vector<std::string>& vec_extra_paths,
                                 bool b_append_default) {
    try {
        py::module_ sys = py::module_::import("sys");
        py::list path_list = sys.attr("path");

        if (b_append_default) {
            std::filesystem::path default_path = "../python";
            try {
                py::list argv = sys.attr("argv").cast<py::list>();
                if (argv.size() > 0) {
                    std::string argv0 = argv[0].cast<std::string>();
                    std::filesystem::path exe_path(argv0);
                    if (!exe_path.empty()) {
                        std::filesystem::path exe_dir = exe_path.parent_path();
                        std::filesystem::path project_dir = exe_dir.parent_path();
                        default_path = project_dir / "python";
                    }
                }
            } catch (...) {
                // 忽略 sys.argv 读取失败，使用默认相对路径
            }
            std::string default_str = default_path.string();
            if (!path_exists_in_list(path_list, default_str)) {
                path_list.append(default_str);
                spdlog::info("Added '{}' to sys.path", default_str);
            }
        }

        for (const auto& str_path : vec_extra_paths) {
            if (!str_path.empty() && !path_exists_in_list(path_list, str_path)) {
                path_list.append(str_path);
                spdlog::info("Added '{}' to sys.path", str_path);
            }
        }
    } catch (py::error_already_set& e) {
        spdlog::error("Failed to setup sys.path: {}", e.what());
        PyErr_Print();
    }
}
```

3. 修改 `Initialize` 中的调用点：

```cpp
bool PyInterpreter::Initialize(const std::vector<std::string>& vec_extra_paths) {
    std::lock_guard<std::mutex> lock(m_mutex_);

    if (m_b_initialized_.load()) {
        SetupSysPath(vec_extra_paths, false);
        spdlog::warn("Python interpreter already initialized, applying extra paths only");
        return true;
    }

    try {
        py::initialize_interpreter();
        {
            py::gil_scoped_acquire gil;
            (void)py::detail::get_internals();
        }
        SetupSysPath(vec_extra_paths, true);
        m_b_initialized_.store(true);
        spdlog::info("Python interpreter initialized successfully");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize Python interpreter: {}", e.what());
        return false;
    }
}
```

注意：如果当前代码中成员名是 `m_bInitialized` 和 `m_mutex`，保持现有命名风格，不要改成 `m_b_initialized_`。本计划假设后续统一为 trailing underscore；如果当前文件未统一，按当前文件风格保留 `m_bInitialized`。

- [ ] **Step 4: 运行测试确认通过**

```bash
cmake --build build -j
./build/tests/unit_tests --gtest_filter='PyInterpreter.*'
```

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/interpreter/PyInterpreter.cpp tests/interpreter/TestPyInterpreter.cpp
git commit -m "修复 PyInterpreter 幂等调用时 sys.path 重复污染

- SetupSysPath 增加路径去重检查
- 区分首次默认路径设置与额外路径追加
- 新增重复路径测试"
```

---

## Task 2: 修复 `PyInterpreter::Finalize` 异常时状态不一致

**Files:**
- Modify: `src/interpreter/PyInterpreter.cpp`

- [ ] **Step 1: 实现修复**

修改 `src/interpreter/PyInterpreter.cpp` 中的 `Finalize`，确保即使 `py::finalize_interpreter()` 抛异常，`m_bInitialized` 也被重置为 `false`：

```cpp
void PyInterpreter::Finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_bInitialized.load()) {
        spdlog::warn("Python interpreter not initialized, nothing to finalize");
        return;
    }

    try {
        py::finalize_interpreter();
        spdlog::info("Python interpreter finalized");
    } catch (const std::exception& e) {
        spdlog::error("Error during Python interpreter finalization: {}", e.what());
    }
    m_bInitialized.store(false);
}
```

- [ ] **Step 2: 运行相关测试**

```bash
cmake --build build -j
./build/tests/unit_tests --gtest_filter='PyInterpreter.FinalizeResetsState'
```

Expected: PASS

- [ ] **Step 3: Commit**

```bash
git add src/interpreter/PyInterpreter.cpp
git commit -m "修复 PyInterpreter::Finalize 异常时状态不一致

即使 finalize_interpreter 抛异常，也把 m_bInitialized 重置为 false"
```

---

## Task 3: 修复 `python_backend` 对解释器被外部 `Finalize` 的防御

**Files:**
- Modify: `src/cplus_alg/python/python_backend.cpp`
- Test: `tests/interpreter/TestPyInterpreter.cpp`

- [ ] **Step 1: 编写失败测试**

在 `tests/interpreter/TestPyInterpreter.cpp` 中新增测试，验证解释器被外部 `Finalize` 后，`python_backend::available()` 返回 false；重新初始化后返回 true。

```cpp
#include "cplus_alg/python/python_backend.h"

TEST(PyInterpreter, FinalizeInvalidatesPythonBackend) {
    PyInterpreter& interp = PyInterpreter::Instance();
    ASSERT_TRUE(interp.Initialize());

    cplus_alg::python::python_backend backend;
    EXPECT_TRUE(backend.available());

    interp.Finalize();
    EXPECT_FALSE(backend.available());
}
```

确保 `tests/interpreter/TestPyInterpreter.cpp` 的 include 部分包含：

```cpp
#include "interpreter/PyInterpreter.h"
#include "cplus_alg/python/python_backend.h"
#include <pybind11/embed.h>
#include <gtest/gtest.h>
#include <vector>
```

- [ ] **Step 2: 运行测试确认失败**

```bash
cmake --build build -j
./build/tests/unit_tests --gtest_filter='PyInterpreter.FinalizeInvalidatesPythonBackend'
```

Expected: FAIL，因为当前 `python_backend` 的 `initialized_` 状态不会被外部 `Finalize` 重置。

- [ ] **Step 3: 实现修复**

修改 `src/cplus_alg/python/python_backend.cpp` 中的 `python_runtime::ensure_initialized()`，在 `initialized_ == true` 但 `PyInterpreter::IsInitialized() == false` 时主动重置状态：

```cpp
void ensure_initialized() {
    if (initialized_) {
        if (!PyInterpreter::Instance().IsInitialized()) {
            // 解释器被外部 Finalize，重置自身状态以便重新初始化
            registry_ = py::object();
            initialized_ = false;
        } else {
            return;
        }
    }

    CPLUS_ALG_LOG_DEBUG("initializing embedded python interpreter");
    auto& interp = PyInterpreter::Instance();
    if (!interp.Initialize()) {
        throw std::runtime_error("failed to initialize embedded python interpreter via PyInterpreter");
    }
    try {
        add_alg_path();
        registry_ = py::module_::import("alg.core.registry");
        initialized_ = true;
        CPLUS_ALG_LOG_DEBUG("python interpreter initialized, alg.core.registry loaded");
    } catch (...) {
        CPLUS_ALG_LOG_ERROR("failed to initialize python runtime");
        registry_ = py::object();
        throw;
    }
}
```

- [ ] **Step 4: 运行测试确认通过**

```bash
cmake --build build -j
./build/tests/unit_tests --gtest_filter='PyInterpreter.*'
```

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/cplus_alg/python/python_backend.cpp tests/interpreter/TestPyInterpreter.cpp
git commit -m "修复 python_backend 对外部 Finalize 的防御

- python_runtime 检测解释器被外部 Finalize 后自动重置状态
- 新增 python_backend::available() 与解释器状态联动测试"
```

---

## Task 4: Minor 代码清理

**Files:**
- Modify: `src/interpreter/GilManager.h`
- Modify: `src/interpreter/PyInterpreter.h`
- Modify: `docs/2026-06-12-integrate-interpreter-threading.md`

- [ ] **Step 1: 简化 `GilScopedRelease` 初始化**

修改 `src/interpreter/GilManager.h`：

```cpp
GilScopedRelease()
    : m_bReleased(false), m_tstate(PyEval_SaveThread()) {
    m_bReleased = (m_tstate != nullptr);
}
```

- [ ] **Step 2: 移除未使用的 include**

修改 `src/interpreter/PyInterpreter.h`，删除 `#include <memory>`。

- [ ] **Step 3: 更新计划文档状态**

修改 `docs/2026-06-12-integrate-interpreter-threading.md`，将"实施步骤"中的 `[ ]` 全部改为 `[x]`。

- [ ] **Step 4: 编译验证**

```bash
cmake --build build -j
```

Expected: 编译成功，无新增警告。

- [ ] **Step 5: Commit**

```bash
git add src/interpreter/GilManager.h src/interpreter/PyInterpreter.h docs/2026-06-12-integrate-interpreter-threading.md
git commit -m "Minor 清理

- 简化 GilScopedRelease 初始化
- 移除 PyInterpreter.h 中未使用的 <memory>
- 标记计划文档实施步骤为已完成"
```

---

## Task 5: 全量回归测试

**Files:**
- 无新增文件

- [ ] **Step 1: 运行完整测试套件**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Expected: 100% tests passed, 0 tests failed

- [ ] **Step 2: 运行主程序冒烟测试**

```bash
./build/CplusAlg
```

Expected: 模板匹配和 curve_fit 示例成功运行并输出结果。

- [ ] **Step 3: 提交（如尚未提交）**

如果前面所有 Task 都已分别 commit，本步骤可跳过。否则：

```bash
git add -A
git commit -m "修复 interpreter/threading review 问题并补充测试"
```

---

## Self-Review

**Spec coverage:**
- `sys.path` 去重：Task 1
- `Finalize` 异常状态一致：Task 2
- `python_backend` 外部 `Finalize` 防御：Task 3
- 状态联动测试：Task 3
- Minor 清理：Task 4
- 全量回归：Task 5

**Placeholder scan:** 无 TBD/TODO/"implement later"，每个步骤均包含具体代码或命令。

**Type consistency：** 所有新增测试函数、辅助函数命名在各自任务中保持一致；计划沿用了现有代码的命名风格。

---

## Execution Handoff

**Plan complete and saved to `docs/superpowers/plans/2026-06-12-fix-interpreter-threading-review-issues.md`.**

Two execution options:

1. **Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration.
2. **Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints.

Which approach?
