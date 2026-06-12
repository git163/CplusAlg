# 修复嵌入式 Python 解释器 teardown 阶段偶现 SIGTRAP

- 日期: 2026-06-12
- 作者: Claude
- 状态: 已实施
- 关联: `src/cplus_alg/python/python_backend.cpp`

## 背景

`ctest` 运行单元测试时，所有调用 Python 后端的用例（`AlgInterface.DirectTransmitTag`、`AutoTransmitChoosesDirectForSmallData`、`TemplateMatch.*`、`AutoDiscovery.CallNewModule` 等）均偶现崩溃。CTest 报告为 `SIGTRAP` 或 `Subprocess aborted`。

关键现象：

- 崩溃发生在测试**已通过**、全局环境 tear-down 之后，进程退出阶段。
- 单进程单独跑同一个用例往往不崩，批量跑 CTest 时容易复现。
- 崩溃不固定在某一个用例，而是所有使用 `python_backend` 的用例随机触发。

## 根因分析

`python_backend` 使用 Meyers singleton 持有内嵌 Python 解释器：

```cpp
static python_runtime& instance() {
    static python_runtime inst;   // 静态局部对象
    return inst;
}
```

`python_runtime` 内部持有 `std::unique_ptr<py::scoped_interpreter> guard_` 和 `py::object registry_`。程序退出时静态析构按声明逆序执行：

1. 先析构 `registry_`（释放对 `alg.core.registry` 模块的引用）。
2. 再析构 `guard_`，调用 `Py_Finalize()` 关闭解释器。

问题出在**静态析构顺序的不确定性**。`Py_Finalize()` 会清理 `sys.modules`、运行 `atexit` 回调、触发垃圾回收。此时：

- pybind11 内部静态状态可能已被部分销毁；
- numpy 等 C 扩展在清理期间可能访问已被释放的 pybind11 内部结构；
-  spdlog 等其它静态单例可能已处于析构中途；

从而触发 `__builtin_trap()`（表现为 `SIGTRAP`），或在更复杂情况下导致 `SEGFAULT`。

## 目标

- 彻底消除 Python 后端用例在 teardown 阶段的偶现崩溃。
- 保持接口与现有行为不变，不引入新的公共 API。
- 不影响正常调用路径的性能与语义。

## 方案

将 `python_runtime` 从**静态局部对象**改为**堆分配并故意不 `delete`**：

```cpp
static python_runtime& instance() {
    static python_runtime* inst = new python_runtime();
    return *inst;
}
```

这样：

- 解释器生命周期仍覆盖整个程序运行期。
- 进程退出时，该单例永远不会被析构，因此不会调用 `Py_Finalize()`，也不会与 pybind11/numpy/spdlog 的静态析构发生顺序冲突。
- 操作系统会在进程退出时回收全部内存、文件描述符、共享内存等资源。

同时保留一个 `shutdown()` 方法，供未来需要显式干净退出的场景（如长时间运行的服务进程）主动调用。

## 影响范围

- 仅修改 `src/cplus_alg/python/python_backend.cpp`。
- `python_backend::dispatch()` 与 `python_backend::available()` 行为不变。
- 单元测试与主程序调用路径不变。

## 风险与对策

| 风险 | 对策 |
|------|------|
| 内存泄漏（解释器未 finalize） | 单例本就应该伴随整个进程生命周期；进程退出时 OS 回收。已在注释中说明这是 intentional leak。 |
| Python `atexit` 回调不执行 | 当前设计未依赖 Python atexit；如需，可显式调用 `python_runtime::shutdown()`。 |
| 共享内存残留 | OS 在进程退出时自动清理该进程创建的 POSIX shared memory 映射与对象。 |

## 替代方案

1. **在 `std::atexit` 中显式 finalize 解释器**  
   已尝试：能减少崩溃频率，但仍偶发 SIGTRAP；且在 atexit 中写日志会触发 spdlog 的静态析构 SEGFAULT。最终放弃。

2. **为每个 dispatch 单独创建/销毁解释器**  
   初始化耗时约 350ms，性能不可接受。

3. **拷贝数据替代零拷贝 memoryview**  
   崩溃发生在解释器关闭阶段，与数据是否零拷贝无关，不能根治。

## 实施步骤

- [x] 复现崩溃并确认发生在进程退出阶段。
- [x] 排除数据生命周期、GIL、numpy memoryview 等方向。
- [x] 将 `python_runtime` 改为堆分配单例并注释原因。
- [x] 保留 `shutdown()` 接口供显式清理。
- [x] 连续运行 `build.sh` / `scripts/run_tests.sh` 30+ 次验证稳定性。

## 测试计划

- [x] `sh scripts/run_tests.sh` 连续 30 次全部通过。
- [x] `sh build.sh`（含 CTest + 主程序）连续 10 次全部通过。
- [x] 单独跑高概率失败的 `TemplateMatch.AutoTransmit`、`AlgInterface.DirectTransmitTag` 各多次，无崩溃。

## 引用

- pybind11 embedded interpreter 文档：`finalize_interpreter()` 与 `scoped_interpreter` 说明。
- C++ 静态析构顺序问题（Static Initialization Order Fiasco 的镜像问题）。
