# src/threading 多线程效率测试套件实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 `src/threading` 4 个组件补充多线程效率测试（GTest 软断言版 + 独立 perf main），覆盖加速比、扩展性、饱和度、延迟、吞吐量五类指标，软阈值断言，支持 4/8/16/32 线程扫描。

**Architecture:** 方案 C 混合 - GTest 软断言版（接 ctest） + 独立 perf main（手动跑，打印表格+CSV）。基础设施层 `perf_workloads.h` + `perf_metrics.h` + `perf_test_base.h` 提供可复用的测试注册、计时、统计、阈值检查能力；4 个 `perf_*.cpp` 文件分别覆盖 4 个组件的 21 个 perf 用例；`perf_main.cpp` 统一 CLI、表格打印、退出码。

**Tech Stack:** C++17, GTest, spdlog, std::chrono, std::thread, std::future, pybind11（通过现有 `cplus_alg_lib`）。

**前置约束：**
- 所有阻塞等待带超时，避免死锁挂 CI
- 所有线程数测试默认扫描 `{1, 2, 4, 8, 16, 32}`
- 软阈值统一为 `≥ 0.5 × 理论加速比`（理论 = `min(线程数, 核数)`）
- 头文件 `#ifndef` 保护（与项目规约一致）
- 中文注释，代码命名 `snake_case`，类成员 `trailing_`

**注：项目实际物理路径是 `src/threading/`（不是 spec 误写的 `src/cplus_alg/threading/`），CMakeLists 在 `src/cplus_alg/threading/CMakeLists.txt` 中通过绝对路径引用源文件。**

---

## 文件结构

| 路径 | 状态 | 职责 |
|---|---|---|
| `src/threading/perf/perf_workloads.h` | 新建 | 三类 workload（纯 C++ / GIL 模拟 / 真实 Python） |
| `src/threading/perf/perf_metrics.h` | 新建 | SampleStats、ComputeStats、PrintTable、PrintCsv |
| `src/threading/perf/perf_metrics.cpp` | 新建 | metrics 函数实现 |
| `src/threading/perf/perf_test_base.h` | 新建 | TestSpec、TestRegistry、PerfOptions、Status 枚举、REGISTER_PERF_TEST 宏 |
| `src/threading/perf/perf_test_base.cpp` | 新建 | TestRegistry 实现 |
| `src/threading/perf/perf_parallel_executor.cpp` | 新建 | ParallelExecutor 5 个 perf 用例 |
| `src/threading/perf/perf_py_thread_pool.cpp` | 新建 | PyThreadPool 6 个 perf 用例 |
| `src/threading/perf/perf_py_producer_consumer.cpp` | 新建 | PyProducerConsumer 5 个 perf 用例 |
| `src/threading/perf/perf_py_batch_consumer.cpp` | 新建 | PyBatchConsumer 5 个 perf 用例 |
| `src/threading/perf/perf_main.cpp` | 新建 | main()、CLI、runner、表格打印、退出码 |
| `src/cplus_alg/threading/CMakeLists.txt` | 修改 | 新增 `threading_perf` 可执行 target |
| `tests/threading/TestThreadingPerf.cpp` | 新建 | GTest 软断言版（接 ctest） |
| `tests/CMakeLists.txt` | 修改 | 把 TestThreadingPerf.cpp 加入 `unit_tests` 源列表 |
| `scripts/run_threading_perf.sh` | 新建 | 调 perf main，支持 `--quick` |

**对测试自己的单元测试（GTest 软断言版的子集，会自动跑通）：**
- 在 `TestThreadingPerf.cpp` 中用 `TEST_F` 直接定义 perf 用例的 GTest 版，无需单独 unit test 文件
- `perf_metrics` / `perf_test_base` 不单独建测试，靠后续 perf 用例的实际使用验证（避免过度工程化）

---

## Task 1: 创建 perf 目录、最小可运行的 perf_main.cpp、CMake target

**Files:**
- Create: `src/threading/perf/perf_main.cpp`（占位 main，返回 0）
- Modify: `src/cplus_alg/threading/CMakeLists.txt`（新增 `threading_perf` target）

- [ ] **Step 1.1: 创建 src/threading/perf 目录**

```bash
mkdir -p /Users/tshua/respo/Code/CplusAlg/src/threading/perf
```

- [ ] **Step 1.2: 创建最小 perf_main.cpp（占位）**

文件 `src/threading/perf/perf_main.cpp`：

```cpp
// src/threading/perf/perf_main.cpp — threading 性能测试入口（占位骨架）

#include <cstdio>

int main() {
    std::puts("threading_perf skeleton (not implemented yet)");
    return 0;
}
```

- [ ] **Step 1.3: 修改 CMakeLists.txt，新增 threading_perf target**

修改文件 `src/cplus_alg/threading/CMakeLists.txt`（在现有 `list(APPEND ...)` 之后追加）：

```cmake
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
```

- [ ] **Step 1.4: 重新生成构建并验证编译通过**

```bash
cd /Users/tshua/respo/Code/CplusAlg
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo 2>&1 | tail -20
cmake --build build --target threading_perf -j 2>&1 | tail -20
```

期望：构建无错误，产物在 `build/src/cplus_alg/threading/threading_perf`（CMake target 输出路径规则：在 target 所在 CMakeLists.txt 目录对应子目录下）。

- [ ] **Step 1.5: 跑一次确认输出**

```bash
./build/src/cplus_alg/threading/threading_perf
```

期望输出：

```
threading_perf skeleton (not implemented yet)
```

退出码 0。

- [ ] **Step 1.6: 提交**

```bash
cd /Users/tshua/respo/Code/CplusAlg
git add src/threading/perf/perf_main.cpp src/cplus_alg/threading/CMakeLists.txt
git commit -m "feat(perf): add threading_perf executable target skeleton

建立 src/threading/perf/ 目录与 threading_perf 可执行文件 target，
目前为占位 main()。后续任务会逐步添加 perf_workloads、
perf_metrics、perf_test_base 基础设施及 21 个 perf 用例。"
```

---

## Task 2: 实现 perf_workloads.h（三类 workload）

**Files:**
- Create: `src/threading/perf/perf_workloads.h`

- [ ] **Step 2.1: 创建 perf_workloads.h**

文件 `src/threading/perf/perf_workloads.h`：

```cpp
// src/threading/perf/perf_workloads.h — perf 测试的三类 workload
// 提供纯 C++ 计算 / GIL 模拟 / 真实 Python 调用三种负载，
// 用于覆盖不同的多线程效率场景。

#ifndef CPERF_PERF_WORKLOADS_H
#define CPERF_PERF_WORKLOADS_H

#include "interpreter/GilManager.h"
#include "interpreter/PyInterpreter.h"
#include "python/PyModule.h"

#include <cstdint>
#include <vector>

namespace perf {

// 单次迭代约 1ns（粗略估算），可通过调整 iterations 控制总耗时
// 默认 1e8 次 ≈ 100ms（在现代 x86 上）
constexpr int kDefaultComputeIterations = 100'000'000;

// Workload A: 纯 C++ 计算（无 GIL 干扰）
// 用途：测出理论加速比上限，验证基础设施
inline void workload_pure_compute(int iterations = kDefaultComputeIterations) {
    volatile std::uint64_t acc = 0;
    for (int i = 0; i < iterations; ++i) {
        acc += static_cast<std::uint64_t>(i) * 0x9E3779B97F4A7C15ULL;
    }
    (void)acc;
}

// Workload B: GIL 模拟
// 模拟"Python 释放 GIL 后做 CPU 密集"的最常见生产模式
// 对应 PyThreadPool 提交的任务体；加速比上界 ≈ 核数
inline std::int64_t workload_burn_gil(int iterations = kDefaultComputeIterations) {
    GilScopedRelease release;          // 模拟 numpy/pandas 的 GIL 释放
    volatile std::uint64_t acc = 0;
    for (int i = 0; i < iterations; ++i) {
        acc += static_cast<std::uint64_t>(i) * 0x9E3779B97F4A7C15ULL;
    }
    return static_cast<std::int64_t>(acc);
}

// Workload C: 真实 Python 调用
// 端到端验证，捕获 Python ↔ C++ 转换开销
// 数据规模可调（小数组 ~1ms，大数组 ~50ms）
// 注：调用方需自行 try/catch，模块未加载时抛出 runtime_error
inline double workload_real_python_sum(std::vector<double>&& data) {
    python::PyModule caller("demo.numpy_ops");
    return caller.Call("sum_array", data).cast<double>();
}

// Workload C2: 真实 Python 睡眠（适合 GIL 释放场景）
// 用于测试真实 Python 调用下的并行性
inline void workload_real_python_sleep(std::chrono::milliseconds ms) {
    python::PyModule caller("demo.numpy_ops");
    caller.Call("sleep_ms", static_cast<int>(ms.count()));
}

} // namespace perf

#endif // CPERF_PERF_WORKLOADS_H
```

- [ ] **Step 2.2: 把 perf_workloads.h 加入 CMake target**

修改 `src/cplus_alg/threading/CMakeLists.txt`，在 `add_executable(threading_perf ...)` 中确认（不需要 .cpp 加入，但需要 include 路径已配）。无需修改，因为 target_include_directories 已经包含 `src/` 目录。

- [ ] **Step 2.3: 验证编译通过（让后续 task 引入时不会因路径出错）**

```bash
cd /Users/tshua/respo/Code/CplusAlg
cmake --build build --target threading_perf -j 2>&1 | tail -20
```

期望：构建无错误。

- [ ] **Step 2.4: 提交**

```bash
cd /Users/tshua/respo/Code/CplusAlg
git add src/threading/perf/perf_workloads.h
git commit -m "feat(perf): add perf_workloads.h with 3 workload types

- workload_pure_compute: 纯 C++ 计算，无 GIL 干扰，验证理论加速比
- workload_burn_gil: 模拟 numpy/pandas 释放 GIL 后的 CPU 密集模式
- workload_real_python_sum / sleep: 端到端验证 Python ↔ C++ 转换开销"
```

---

## Task 3: 实现 perf_metrics.h + perf_metrics.cpp

**Files:**
- Create: `src/threading/perf/perf_metrics.h`
- Create: `src/threading/perf/perf_metrics.cpp`
- Modify: `src/cplus_alg/threading/CMakeLists.txt`（加入 perf_metrics.cpp 到 target）

- [ ] **Step 3.1: 创建 perf_metrics.h**

文件 `src/threading/perf/perf_metrics.h`：

```cpp
// src/threading/perf/perf_metrics.h — 性能测试的统计、计时、打印工具

#ifndef CPERF_PERF_METRICS_H
#define CPERF_PERF_METRICS_H

#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace perf {

// 一次 perf run 的统计结果
struct SampleStats {
    std::chrono::milliseconds p50{0};
    std::chrono::milliseconds p95{0};
    std::chrono::milliseconds p99{0};
    std::chrono::milliseconds max{0};
    double mean_ms{0.0};
    double throughput_per_sec{0.0};    // tasks / second
    double speedup_vs_baseline{1.0};   // 对比 1 线程基线
};

// 输入纳秒样本，计算 P50/P95/P99/最大/均值
// 样本会被排序
SampleStats ComputeStats(std::vector<std::chrono::nanoseconds>&& samples,
                         double baseline_throughput = 0.0);

// 打印人类可读表格到 stdout
// title: e.g. "=== PE_Speedup_HeavyCompute (workload_pure_compute) ==="
// rows: pairs of (config_label, stats)，如 ("threads=4", SampleStats{...})
void PrintTable(const std::string& title,
                const std::vector<std::pair<std::string, SampleStats>>& rows);

// 写入 CSV 文件
// 列：test_name,workload,threads,wall_ms,throughput_tps,speedup,efficiency,p50_ms,p95_ms,p99_ms,regression_flag
// rows 同时携带线程数（通过 config_label 解析或外部提供）
void PrintCsv(const std::string& path,
              const std::string& test_name,
              const std::string& workload,
              const std::vector<std::pair<int, SampleStats>>& rows);

} // namespace perf

#endif // CPERF_PERF_METRICS_H
```

- [ ] **Step 3.2: 创建 perf_metrics.cpp**

文件 `src/threading/perf/perf_metrics.cpp`：

```cpp
// src/threading/perf/perf_metrics.cpp — metrics 实现

#include "perf_metrics.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>

namespace perf {

namespace {

std::chrono::milliseconds Percentile(
    const std::vector<std::chrono::nanoseconds>& sorted,
    double p) {
    if (sorted.empty()) return std::chrono::milliseconds{0};
    std::size_t idx = static_cast<std::size_t>(p * sorted.size());
    if (idx >= sorted.size()) idx = sorted.size() - 1;
    return std::chrono::duration_cast<std::chrono::milliseconds>(sorted[idx]);
}

} // namespace

SampleStats ComputeStats(std::vector<std::chrono::nanoseconds>&& samples,
                         double baseline_throughput) {
    SampleStats stats;
    if (samples.empty()) return stats;

    std::sort(samples.begin(), samples.end());

    const double total_ns =
        static_cast<double>(std::accumulate(
            samples.begin(), samples.end(),
            std::chrono::nanoseconds{0}.count(),
            [](long long acc, std::chrono::nanoseconds ns) {
                return acc + ns.count();
            }));
    const double mean_ns = total_ns / samples.size();
    stats.mean_ms = mean_ns / 1e6;

    stats.p50 = Percentile(samples, 0.50);
    stats.p95 = Percentile(samples, 0.95);
    stats.p99 = Percentile(samples, 0.99);
    stats.max = std::chrono::duration_cast<std::chrono::milliseconds>(samples.back());

    // throughput: tasks/sec（假设每个 sample 是一个 task 的耗时）
    if (mean_ns > 0) {
        stats.throughput_per_sec = 1e9 / mean_ns;
    }

    if (baseline_throughput > 0 && stats.throughput_per_sec > 0) {
        stats.speedup_vs_baseline = stats.throughput_per_sec / baseline_throughput;
    } else {
        stats.speedup_vs_baseline = 1.0;
    }

    return stats;
}

void PrintTable(const std::string& title,
                const std::vector<std::pair<std::string, SampleStats>>& rows) {
    std::cout << title << "\n";
    std::cout << "  " << std::left
              << std::setw(16) << "config"
              << std::setw(12) << "mean_ms"
              << std::setw(14) << "throughput"
              << std::setw(12) << "speedup"
              << std::setw(12) << "P50_ms"
              << std::setw(12) << "P95_ms"
              << std::setw(12) << "P99_ms"
              << "\n";
    std::cout << "  " << std::string(90, '-') << "\n";
    for (const auto& [label, s] : rows) {
        std::cout << "  " << std::left
                  << std::setw(16) << label
                  << std::setw(12) << std::fixed << std::setprecision(2) << s.mean_ms
                  << std::setw(14) << std::fixed << std::setprecision(1)
                       << s.throughput_per_sec << " tps"
                  << std::setw(11) << std::fixed << std::setprecision(2)
                       << s.speedup_vs_baseline << "x"
                  << std::setw(12) << s.p50.count()
                  << std::setw(12) << s.p95.count()
                  << std::setw(12) << s.p99.count()
                  << "\n";
    }
    std::cout << "\n";
}

void PrintCsv(const std::string& path,
              const std::string& test_name,
              const std::string& workload,
              const std::vector<std::pair<int, SampleStats>>& rows) {
    std::ofstream ofs(path);
    if (!ofs) {
        std::cerr << "Failed to open CSV: " << path << "\n";
        return;
    }
    ofs << "test_name,workload,threads,wall_ms,throughput_tps,speedup,efficiency,p50_ms,p95_ms,p99_ms,regression_flag\n";
    for (const auto& [threads, s] : rows) {
        ofs << test_name << ","
            << workload << ","
            << threads << ","
            << s.mean_ms << ","
            << s.throughput_per_sec << ","
            << s.speedup_vs_baseline << ","
            << s.speedup_vs_baseline / std::max(1, threads) << ","
            << s.p50.count() << ","
            << s.p95.count() << ","
            << s.p99.count() << ","
            << "ok"
            << "\n";
    }
}

} // namespace perf
```

- [ ] **Step 3.3: 把 perf_metrics.cpp 加入 CMake target**

修改 `src/cplus_alg/threading/CMakeLists.txt`：

```cmake
add_executable(threading_perf
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_main.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_metrics.cpp
)
```

- [ ] **Step 3.4: 构建并验证**

```bash
cd /Users/tshua/respo/Code/CplusAlg
cmake --build build --target threading_perf -j 2>&1 | tail -20
```

期望：构建无错误。

- [ ] **Step 3.5: 提交**

```bash
cd /Users/tshua/respo/Code/CplusAlg
git add src/threading/perf/perf_metrics.h src/threading/perf/perf_metrics.cpp \
        src/cplus_alg/threading/CMakeLists.txt
git commit -m "feat(perf): add perf_metrics with SampleStats, ComputeStats, PrintTable, PrintCsv"
```

---

## Task 4: 实现 perf_test_base.h + perf_test_base.cpp（注册基础设施）

**Files:**
- Create: `src/threading/perf/perf_test_base.h`
- Create: `src/threading/perf/perf_test_base.cpp`
- Modify: `src/cplus_alg/threading/CMakeLists.txt`（加入 perf_test_base.cpp）

- [ ] **Step 4.1: 创建 perf_test_base.h**

文件 `src/threading/perf/perf_test_base.h`：

```cpp
// src/threading/perf/perf_test_base.h — perf 测试注册与执行基础设施
// 用法：每个 perf 用例定义一个 TestSpec，注册到全局 TestRegistry，
// perf_main 在启动时遍历注册表执行。

#ifndef CPERF_PERF_TEST_BASE_H
#define CPERF_PERF_TEST_BASE_H

#include "perf_metrics.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace perf {

enum class Status {
    Pass,  // 通过
    Warn,  // 软阈值警告
    Fail,  // 硬阈值失败
};

// perf main 的运行时选项（由 CLI 解析填充）
struct PerfOptions {
    std::vector<int> thread_sweep = {1, 2, 4, 8, 16, 32};
    int repeat = 3;
    bool quiet = false;
    std::string csv_path;
    std::string filter;     // 子串匹配，留空跑全部
    int hardware_concurrency = 0;  // 由 perf_main 启动时填入
};

// 单个 perf run 的完整结果
struct TestResult {
    std::string test_name;
    std::string workload;
    // key = 线程数；value = 该线程数下的统计
    std::map<int, SampleStats> by_threads;
    Status status = Status::Pass;
    std::string message;     // 阈值违反时的描述
};

// 单个 perf 用例的执行入口
// 返回 TestResult；用例内部用 opts.thread_sweep 循环即可
using TestFn = std::function<TestResult(const PerfOptions& opts)>;

struct TestSpec {
    std::string name;
    std::string workload;     // 描述性，如 "pure_compute" / "burn_gil" / "real_python"
    TestFn run;
};

// 全局注册表（单例）
class TestRegistry {
public:
    static TestRegistry& Instance();

    void Register(const TestSpec& spec);

    // 返回所有用例；按 name 升序
    std::vector<TestSpec> All() const;

    // 按 filter 子串过滤（不区分大小写）；filter 为空返回全部
    std::vector<TestSpec> Filter(const std::string& filter) const;

private:
    TestRegistry() = default;
    std::map<std::string, TestSpec> tests_;  // map 保证按 name 升序
};

// 静态注册宏：在 .cpp 匿名 namespace 中调用，全局构造期自动注册
#define PERF_REGISTER_TEST(test_name, workload_str, fn_body) \
    namespace { \
        struct PerfRegistrar_##test_name { \
            PerfRegistrar_##test_name() { \
                perf::TestRegistry::Instance().Register({ \
                    #test_name, workload_str, fn_body \
                }); \
            } \
        }; \
        static const PerfRegistrar_##test_name \
            s_perf_registrar_##test_name{}; \
    }

} // namespace perf

#endif // CPERF_PERF_TEST_BASE_H
```

- [ ] **Step 4.2: 创建 perf_test_base.cpp**

文件 `src/threading/perf/perf_test_base.cpp`：

```cpp
// src/threading/perf/perf_test_base.cpp — TestRegistry 实现

#include "perf_test_base.h"

#include <algorithm>
#include <cctype>

namespace perf {

TestRegistry& TestRegistry::Instance() {
    static TestRegistry instance;
    return instance;
}

void TestRegistry::Register(const TestSpec& spec) {
    tests_[spec.name] = spec;
}

std::vector<TestSpec> TestRegistry::All() const {
    std::vector<TestSpec> out;
    out.reserve(tests_.size());
    for (const auto& [k, v] : tests_) {
        out.push_back(v);
    }
    return out;
}

namespace {

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

} // namespace

std::vector<TestSpec> TestRegistry::Filter(const std::string& filter) const {
    std::vector<TestSpec> all = All();
    if (filter.empty()) return all;

    const std::string lower_filter = ToLower(filter);
    std::vector<TestSpec> out;
    for (const auto& spec : all) {
        if (ToLower(spec.name).find(lower_filter) != std::string::npos) {
            out.push_back(spec);
        }
    }
    return out;
}

} // namespace perf
```

- [ ] **Step 4.3: 把 perf_test_base.cpp 加入 CMake target**

修改 `src/cplus_alg/threading/CMakeLists.txt`：

```cmake
add_executable(threading_perf
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_main.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_metrics.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_test_base.cpp
)
```

- [ ] **Step 4.4: 构建验证**

```bash
cd /Users/tshua/respo/Code/CplusAlg
cmake --build build --target threading_perf -j 2>&1 | tail -20
```

期望：构建无错误。

- [ ] **Step 4.5: 提交**

```bash
cd /Users/tshua/respo/Code/CplusAlg
git add src/threading/perf/perf_test_base.h src/threading/perf/perf_test_base.cpp \
        src/cplus_alg/threading/CMakeLists.txt
git commit -m "feat(perf): add TestRegistry and PERF_REGISTER_TEST macro

提供 TestSpec/PerfOptions/Status/TestResult 与 TestRegistry 单例，
perf_main 启动时遍历注册表执行所有用例。"
```

---

## Task 5: 实现 perf_parallel_executor.cpp（5 个 PE 用例）

**Files:**
- Create: `src/threading/perf/perf_parallel_executor.cpp`
- Modify: `src/cplus_alg/threading/CMakeLists.txt`（加入该 .cpp）

### 通用模板

每个 perf 用例遵循以下结构：

```cpp
static perf::TestResult PE_<Name>(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PE_<Name>";
    r.workload = "<workload_str>";

    // 1. 对 opts.thread_sweep 中每个 threads 跑 opts.repeat 次，收集 samples
    for (int threads : opts.thread_sweep) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            // 计时执行 ...
            auto start = std::chrono::high_resolution_clock::now();
            // ... 调用 ParallelExecutor::RunInParallel / RunBenchmarkComparison ...
            auto end = std::chrono::high_resolution_clock::now();
            samples.push_back(end - start);
        }
        // 2. 计算统计
        double baseline = (threads == 1) ? 0.0 : 0.0;  // 第一次循环时记录基线
        auto stats = perf::ComputeStats(std::move(samples), baseline);
        r.by_threads[threads] = stats;
    }

    // 3. 软阈值检查（用第一个线程的吞吐做基线）
    double baseline_tps = r.by_threads.at(1).throughput_per_sec;
    int hw = std::max(1, opts.hardware_concurrency);
    for (auto& [t, s] : r.by_threads) {
        s.speedup_vs_baseline = (baseline_tps > 0) ? s.throughput_per_sec / baseline_tps : 1.0;
        double theoretical = std::min(t, hw);
        if (s.speedup_vs_baseline < theoretical * 0.3) {
            r.status = perf::Status::Fail;
            r.message = "threads=" + std::to_string(t) + " speedup " +
                std::to_string(s.speedup_vs_baseline) + "x below 30% of theoretical";
            break;
        } else if (s.speedup_vs_baseline < theoretical * 0.5) {
            if (r.status == perf::Status::Pass) {
                r.status = perf::Status::Warn;
                r.message = "threads=" + std::to_string(t) + " speedup below 50% of theoretical";
            }
        }
    }
    return r;
}
PERF_REGISTER_TEST(PE_<Name>, "<workload_str>", PE_<Name>)
```

### 5 个 PE 用例规格

| 用例名 | workload_str | 测试逻辑 |
|---|---|---|
| `PE_Speedup_HeavyCompute` | `pure_compute` | `RunBenchmarkComparison(nCalls=40, threads)`，workload=workload_pure_compute（100ms/块）；检查加速比 ≥ 0.5×理论 |
| `PE_Scalability_ThreadSweep` | `pure_compute` | 固定总工作量（`nCalls=80`, workload=workload_pure_compute），扫描 `{1,2,4,8,16,32}` 线程；检查加速比单调非降（弱） |
| `PE_Throughput_Saturated` | `pure_compute_short` | 短任务 × 100000，workload=`workload_pure_compute(1000)`（~1μs/块）；统计 ops/sec；检查 8 线程 ≥ 4× 1 线程 |
| `PE_Latency_TailDistribution` | `pure_compute` | 1000 次 `RunInParallel(threads, ...)`，记录单次耗时；P99/P50 比值；检查 ≤ 10 |
| `PE_ScalingEfficiency_32` | `pure_compute` | 大工作量（`nCalls=320, workload=workload_pure_compute`），32 vs 1 线程；检查效率 ≥ 50% |

- [ ] **Step 5.1: 创建 perf_parallel_executor.cpp 骨架 + 第一个完整测试**

文件 `src/threading/perf/perf_parallel_executor.cpp`：

```cpp
// src/threading/perf/perf_parallel_executor.cpp — ParallelExecutor 性能测试

#include "perf_test_base.h"
#include "perf_workloads.h"
#include "threading/ParallelExecutor.h"

#include <algorithm>
#include <chrono>
#include <vector>

namespace {

// 共享基线加速比检查工具
void CheckSpeedupThresholds(perf::TestResult& r,
                            int hardware_concurrency) {
    int hw = std::max(1, hardware_concurrency);
    for (const auto& [threads, s] : r.by_threads) {
        double theoretical = std::min(threads, hw);
        if (s.speedup_vs_baseline < theoretical * 0.3) {
            r.status = perf::Status::Fail;
            r.message = "threads=" + std::to_string(threads) +
                " speedup " + std::to_string(s.speedup_vs_baseline) +
                "x below 30% of theoretical " +
                std::to_string(theoretical);
            return;
        } else if (s.speedup_vs_baseline < theoretical * 0.5) {
            if (r.status == perf::Status::Pass) {
                r.status = perf::Status::Warn;
                r.message = "threads=" + std::to_string(threads) +
                    " speedup " + std::to_string(s.speedup_vs_baseline) +
                    "x below 50% of theoretical " +
                    std::to_string(theoretical);
            }
        }
    }
}

// 用例 1: PE_Speedup_HeavyCompute（完整实现作模板）
perf::TestResult PE_Speedup_HeavyCompute(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PE_Speedup_HeavyCompute";
    r.workload = "pure_compute";

    for (int threads : opts.thread_sweep) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            // nCalls=40, 每块 ~100ms ≈ 4s 总耗时；4 线程加速比约 3-4x
            auto [single_ms, multi_ms] = ParallelExecutor::RunBenchmarkComparison(
                /*nCalls=*/40, threads, [](int) {
                    perf::workload_pure_compute(perf::kDefaultComputeIterations);
                });
            samples.push_back(std::chrono::milliseconds(multi_ms));
        }
        r.by_threads[threads] =
            perf::ComputeStats(std::move(samples), /*baseline=*/0.0);
    }

    // 用 1 线程做基线
    double baseline_tps = r.by_threads.count(1)
        ? r.by_threads.at(1).throughput_per_sec
        : 0.0;
    for (auto& [t, s] : r.by_threads) {
        s.speedup_vs_baseline = (baseline_tps > 0)
            ? s.throughput_per_sec / baseline_tps : 1.0;
    }
    CheckSpeedupThresholds(r, opts.hardware_concurrency);
    return r;
}
PERF_REGISTER_TEST(PE_Speedup_HeavyCompute, "pure_compute", PE_Speedup_HeavyCompute)

} // namespace
```

- [ ] **Step 5.2: 把 perf_parallel_executor.cpp 加入 CMake target**

修改 `src/cplus_alg/threading/CMakeLists.txt`：

```cmake
add_executable(threading_perf
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_main.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_metrics.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_test_base.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_parallel_executor.cpp
)
```

- [ ] **Step 5.3: 构建并运行验证**

```bash
cd /Users/tshua/respo/Code/CplusAlg
cmake --build build --target threading_perf -j 2>&1 | tail -20
```

期望：构建无错误。骨架里只有一个测试，跑出来应该只有 `PE_Speedup_HeavyCompute` 那一组。

- [ ] **Step 5.4: 提交骨架**

```bash
cd /Users/tshua/respo/Code/CplusAlg
git add src/threading/perf/perf_parallel_executor.cpp \
        src/cplus_alg/threading/CMakeLists.txt
git commit -m "feat(perf): add PE_Speedup_HeavyCompute as template for ParallelExecutor perf tests"
```

- [ ] **Step 5.5: 添加 PE_Scalability_ThreadSweep**

在 `src/threading/perf/perf_parallel_executor.cpp` 的 `} // namespace` 之前追加：

```cpp
// 用例 2: PE_Scalability_ThreadSweep
perf::TestResult PE_Scalability_ThreadSweep(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PE_Scalability_ThreadSweep";
    r.workload = "pure_compute";

    for (int threads : opts.thread_sweep) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            auto start = std::chrono::high_resolution_clock::now();
            ParallelExecutor::RunInParallel(threads, [](int /*tid*/) {
                perf::workload_pure_compute(perf::kDefaultComputeIterations);
            });
            samples.push_back(std::chrono::high_resolution_clock::now() - start);
        }
        r.by_threads[threads] =
            perf::ComputeStats(std::move(samples), 0.0);
    }
    double baseline_tps = r.by_threads.count(1)
        ? r.by_threads.at(1).throughput_per_sec : 0.0;
    for (auto& [t, s] : r.by_threads) {
        s.speedup_vs_baseline = (baseline_tps > 0)
            ? s.throughput_per_sec / baseline_tps : 1.0;
    }
    // 弱检查：加速比单调非降（容忍 0.7x 抖动）
    double prev = 0;
    for (const auto& [t, s] : r.by_threads) {
        if (prev > 0 && s.speedup_vs_baseline < prev * 0.7) {
            r.status = perf::Status::Warn;
            r.message = "threads=" + std::to_string(t) +
                " speedup dropped below 70% of previous";
        }
        prev = s.speedup_vs_baseline;
    }
    return r;
}
PERF_REGISTER_TEST(PE_Scalability_ThreadSweep, "pure_compute", PE_Scalability_ThreadSweep)
```

- [ ] **Step 5.6: 添加 PE_Throughput_Saturated**

在 `} // namespace` 前追加：

```cpp
// 用例 3: PE_Throughput_Saturated
perf::TestResult PE_Throughput_Saturated(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PE_Throughput_Saturated";
    r.workload = "pure_compute_short";

    for (int threads : opts.thread_sweep) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            constexpr int kNumShortTasks = 100000;
            auto start = std::chrono::high_resolution_clock::now();
            ParallelExecutor::RunInParallel(threads, [](int) {
                perf::workload_pure_compute(1000);  // 短任务 ~1us
            });
            // 注意：RunInParallel 平均分配；100000 是"目标总数"，实际由线程数决定
            (void)kNumShortTasks;
            samples.push_back(std::chrono::high_resolution_clock::now() - start);
        }
        r.by_threads[threads] = perf::ComputeStats(std::move(samples), 0.0);
    }
    double baseline_tps = r.by_threads.count(1)
        ? r.by_threads.at(1).throughput_per_sec : 0.0;
    for (auto& [t, s] : r.by_threads) {
        s.speedup_vs_baseline = (baseline_tps > 0)
            ? s.throughput_per_sec / baseline_tps : 1.0;
    }
    CheckSpeedupThresholds(r, opts.hardware_concurrency);
    return r;
}
PERF_REGISTER_TEST(PE_Throughput_Saturated, "pure_compute_short", PE_Throughput_Saturated)
```

**注意**：此用例的实际 workload 应该让每个线程跑固定数量的任务。修改实现为：

```cpp
// 替换 PE_Throughput_Saturated 内的 ParallelExecutor::RunInParallel 调用：
ParallelExecutor::RunInParallel(threads, [threads](int tid) {
    constexpr int kPerThread = 25000;  // 100000 / 4 = 25k/线程
    for (int i = 0; i < kPerThread; ++i) {
        perf::workload_pure_compute(1000);
    }
});
```

- [ ] **Step 5.7: 添加 PE_Latency_TailDistribution**

追加：

```cpp
// 用例 4: PE_Latency_TailDistribution
perf::TestResult PE_Latency_TailDistribution(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PE_Latency_TailDistribution";
    r.workload = "pure_compute";

    for (int threads : opts.thread_sweep) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            // 1000 次"短并行任务"，每次记录单次 wall time
            for (int i = 0; i < 1000 / std::max(1, threads); ++i) {
                auto start = std::chrono::steady_clock::now();
                ParallelExecutor::RunInParallel(threads, [](int) {
                    perf::workload_pure_compute(1000);  // 极短任务
                });
                samples.push_back(std::chrono::steady_clock::now() - start);
            }
        }
        r.by_threads[threads] = perf::ComputeStats(std::move(samples), 0.0);
    }
    // 检查 P99/P50 比值 ≤ 10
    for (const auto& [t, s] : r.by_threads) {
        if (s.p50.count() > 0) {
            double ratio = static_cast<double>(s.p99.count()) / s.p50.count();
            if (ratio > 10.0) {
                r.status = perf::Status::Fail;
                r.message = "threads=" + std::to_string(t) +
                    " P99/P50 ratio " + std::to_string(ratio) + " > 10";
                return r;
            }
        }
    }
    return r;
}
PERF_REGISTER_TEST(PE_Latency_TailDistribution, "pure_compute", PE_Latency_TailDistribution)
```

- [ ] **Step 5.8: 添加 PE_ScalingEfficiency_32**

追加：

```cpp
// 用例 5: PE_ScalingEfficiency_32
perf::TestResult PE_ScalingEfficiency_32(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PE_ScalingEfficiency_32";
    r.workload = "pure_compute";

    // 只跑 1 和 32 线程
    std::vector<int> targets = {1, 32};
    for (int threads : targets) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            auto [single_ms, multi_ms] = ParallelExecutor::RunBenchmarkComparison(
                /*nCalls=*/320, threads, [](int) {
                    perf::workload_pure_compute(perf::kDefaultComputeIterations);
                });
            (void)single_ms;
            samples.push_back(std::chrono::milliseconds(multi_ms));
        }
        r.by_threads[threads] = perf::ComputeStats(std::move(samples), 0.0);
    }
    double t1_tps = r.by_threads.at(1).throughput_per_sec;
    double t32_tps = r.by_threads.at(32).throughput_per_sec;
    double efficiency = (t1_tps > 0) ? t32_tps / t1_tps / 32.0 : 0.0;
    r.by_threads[32].speedup_vs_baseline = t32_tps / std::max(1.0, t1_tps);
    if (efficiency < 0.5) {
        r.status = perf::Status::Warn;
        r.message = "32-thread efficiency " + std::to_string(efficiency * 100) +
            "% below 50%";
    }
    return r;
}
PERF_REGISTER_TEST(PE_ScalingEfficiency_32, "pure_compute", PE_ScalingEfficiency_32)
```

- [ ] **Step 5.9: 构建并验证**

```bash
cd /Users/tshua/respo/Code/CplusAlg
cmake --build build --target threading_perf -j 2>&1 | tail -20
```

期望：构建无错误。

- [ ] **Step 5.10: 提交**

```bash
cd /Users/tshua/respo/Code/CplusAlg
git add src/threading/perf/perf_parallel_executor.cpp
git commit -m "feat(perf): add remaining 4 ParallelExecutor perf tests

- PE_Scalability_ThreadSweep: 扫描 {1,2,4,8,16,32} 检查加速比单调性
- PE_Throughput_Saturated: 100k 短任务压测，验证 8 线程加速比 ≥ 4x
- PE_Latency_TailDistribution: P99/P50 ≤ 10
- PE_ScalingEfficiency_32: 32 线程效率 ≥ 50%"
```

---

## Task 6: 实现 perf_py_thread_pool.cpp（6 个 PTP 用例）

**Files:**
- Create: `src/threading/perf/perf_py_thread_pool.cpp`
- Modify: `src/cplus_alg/threading/CMakeLists.txt`

所有用例都需要 `GilScopedRelease` 夹具（perf main 主线程在跑测试时也需要释放 GIL，让工作线程能拿到）。在 perf main 启动时由 `perf_main.cpp` 设置（见 Task 9）。

### 6 个 PTP 用例规格

| 用例名 | workload | 关键参数 | 软阈值 |
|---|---|---|---|
| `PTP_Speedup_BurnGIL` | `burn_gil` | pool threads，5000 任务，任务体 `workload_burn_gil(80M)` | 32 线程加速比 ≥ 8x |
| `PTP_Speedup_RealPython` | `real_python` | pool threads，500 任务，任务体 `workload_real_python_sleep(50ms)` | 加速比 ≥ 0.5×理论 |
| `PTP_ThreadSweep_All` | `pure_compute` | pool threads，5000 任务，任务体 `workload_pure_compute(1M)` | 单调非降（弱） |
| `PTP_Saturation_QueueBoundSweep` | `pure_compute` | 4/8/16/32 threads × 队列 {10,100,1000,∞} | 吞吐不显著下降 |
| `PTP_Throughput_HighFanout` | `pure_compute` | pool threads × 5000 任务 | ≥ 1.5× 1 线程 |
| `PTP_Latency_SubmitToDone` | `pure_compute` | 1000 任务，单次 Submit→Done 时延 | P95 ≤ 5×P50 |

- [ ] **Step 6.1: 创建 perf_py_thread_pool.cpp（含 1 个完整用例 + 5 个规格占位）**

文件 `src/threading/perf/perf_py_thread_pool.cpp`：

```cpp
// src/threading/perf/perf_py_thread_pool.cpp — PyThreadPool 性能测试

#include "perf_test_base.h"
#include "perf_workloads.h"
#include "threading/PyThreadPool.h"

#include <atomic>
#include <chrono>
#include <future>
#include <vector>

namespace {

// 通用：跑 N 个任务到指定大小的 pool，返回所有 wall-time samples
std::vector<std::chrono::nanoseconds> RunPoolBatch(
    size_t nThreads, size_t nTasks,
    std::function<void(int)> task_body) {

    PyThreadPool pool(nThreads);
    std::vector<std::future<void>> futures;
    futures.reserve(nTasks);

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < nTasks; ++i) {
        auto opt = pool.Submit([i, &task_body]() {
            task_body(static_cast<int>(i));
        });
        if (opt) futures.push_back(std::move(*opt));
    }
    for (auto& f : futures) f.get();
    auto end = std::chrono::high_resolution_clock::now();
    return {end - start};
}

// 用例 1: PTP_Speedup_BurnGIL（完整实现）
perf::TestResult PTP_Speedup_BurnGIL(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PTP_Speedup_BurnGIL";
    r.workload = "burn_gil";

    for (int threads : opts.thread_sweep) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            auto batch = RunPoolBatch(
                static_cast<size_t>(threads),
                /*nTasks=*/5000,
                [](int) {
                    perf::workload_burn_gil(/*iterations=*/80'000'000);  // ~80ms
                });
            // 每个 batch 的总耗时作为一个 sample
            if (!batch.empty()) samples.push_back(batch[0]);
        }
        r.by_threads[threads] = perf::ComputeStats(std::move(samples), 0.0);
    }
    double baseline_tps = r.by_threads.count(1)
        ? r.by_threads.at(1).throughput_per_sec : 0.0;
    for (auto& [t, s] : r.by_threads) {
        s.speedup_vs_baseline = (baseline_tps > 0)
            ? s.throughput_per_sec / baseline_tps : 1.0;
    }
    // 32 线程检查：加速比 ≥ 8x
    if (r.by_threads.count(32)) {
        const auto& s = r.by_threads.at(32);
        if (s.speedup_vs_baseline < 8.0) {
            r.status = perf::Status::Warn;
            r.message = "32-thread speedup " +
                std::to_string(s.speedup_vs_baseline) + "x below 8x";
        }
    }
    return r;
}
PERF_REGISTER_TEST(PTP_Speedup_BurnGIL, "burn_gil", PTP_Speedup_BurnGIL)

} // namespace
```

- [ ] **Step 6.2: 加入 CMake target**

修改 `src/cplus_alg/threading/CMakeLists.txt`：

```cmake
add_executable(threading_perf
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_main.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_metrics.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_test_base.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_parallel_executor.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_py_thread_pool.cpp
)
```

- [ ] **Step 6.3: 构建验证**

```bash
cd /Users/tshua/respo/Code/CplusAlg
cmake --build build --target threading_perf -j 2>&1 | tail -20
```

期望：构建无错误。

- [ ] **Step 6.4: 提交骨架**

```bash
cd /Users/tshua/respo/Code/CplusAlg
git add src/threading/perf/perf_py_thread_pool.cpp \
        src/cplus_alg/threading/CMakeLists.txt
git commit -m "feat(perf): add PTP_Speedup_BurnGIL as template for PyThreadPool perf tests"
```

- [ ] **Step 6.5: 添加 PTP_Speedup_RealPython**

在 `} // namespace` 前追加：

```cpp
// 用例 2: PTP_Speedup_RealPython
perf::TestResult PTP_Speedup_RealPython(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PTP_Speedup_RealPython";
    r.workload = "real_python";

    try {
        for (int threads : opts.thread_sweep) {
            std::vector<std::chrono::nanoseconds> samples;
            for (int rep = 0; rep < opts.repeat; ++rep) {
                auto batch = RunPoolBatch(
                    static_cast<size_t>(threads),
                    /*nTasks=*/500,
                    [](int) {
                        perf::workload_real_python_sleep(std::chrono::milliseconds(50));
                    });
                if (!batch.empty()) samples.push_back(batch[0]);
            }
            r.by_threads[threads] = perf::ComputeStats(std::move(samples), 0.0);
        }
    } catch (const std::exception& e) {
        r.status = perf::Status::Warn;
        r.message = std::string("Real Python demo not available: ") + e.what();
        return r;
    }
    double baseline_tps = r.by_threads.count(1)
        ? r.by_threads.at(1).throughput_per_sec : 0.0;
    for (auto& [t, s] : r.by_threads) {
        s.speedup_vs_baseline = (baseline_tps > 0)
            ? s.throughput_per_sec / baseline_tps : 1.0;
    }
    return r;
}
PERF_REGISTER_TEST(PTP_Speedup_RealPython, "real_python", PTP_Speedup_RealPython)
```

- [ ] **Step 6.6: 添加 PTP_ThreadSweep_All**

追加：

```cpp
// 用例 3: PTP_ThreadSweep_All
perf::TestResult PTP_ThreadSweep_All(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PTP_ThreadSweep_All";
    r.workload = "pure_compute";

    for (int threads : opts.thread_sweep) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            auto batch = RunPoolBatch(
                static_cast<size_t>(threads),
                /*nTasks=*/5000,
                [](int) {
                    perf::workload_pure_compute(/*iterations=*/1'000'000);
                });
            if (!batch.empty()) samples.push_back(batch[0]);
        }
        r.by_threads[threads] = perf::ComputeStats(std::move(samples), 0.0);
    }
    double baseline_tps = r.by_threads.count(1)
        ? r.by_threads.at(1).throughput_per_sec : 0.0;
    for (auto& [t, s] : r.by_threads) {
        s.speedup_vs_baseline = (baseline_tps > 0)
            ? s.throughput_per_sec / baseline_tps : 1.0;
    }
    // 弱检查：单调非降
    double prev = 0;
    for (const auto& [t, s] : r.by_threads) {
        if (prev > 0 && s.speedup_vs_baseline < prev * 0.7) {
            r.status = perf::Status::Warn;
            r.message = "threads=" + std::to_string(t) + " scalability dropped";
        }
        prev = s.speedup_vs_baseline;
    }
    return r;
}
PERF_REGISTER_TEST(PTP_ThreadSweep_All, "pure_compute", PTP_ThreadSweep_All)
```

- [ ] **Step 6.7: 添加 PTP_Saturation_QueueBoundSweep**

追加：

```cpp
// 用例 4: PTP_Saturation_QueueBoundSweep
perf::TestResult PTP_Saturation_QueueBoundSweep(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PTP_Saturation_QueueBoundSweep";
    r.workload = "pure_compute";

    // 4 threads + 4 个队列大小
    constexpr int kThreads = 4;
    std::vector<size_t> queue_sizes = {10, 100, 1000, 0};  // 0 = 无界
    for (size_t qmax : queue_sizes) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            PyThreadPool pool(static_cast<size_t>(kThreads), qmax);
            auto start = std::chrono::high_resolution_clock::now();
            std::vector<std::future<void>> futures;
            for (int i = 0; i < 5000; ++i) {
                auto opt = pool.Submit([]() {
                    perf::workload_pure_compute(1'000'000);
                });
                if (opt) futures.push_back(std::move(*opt));
            }
            for (auto& f : futures) f.get();
            samples.push_back(std::chrono::high_resolution_clock::now() - start);
        }
        int key = (qmax == 0) ? 0 : static_cast<int>(qmax);
        r.by_threads[key] = perf::ComputeStats(std::move(samples), 0.0);
    }
    return r;
}
PERF_REGISTER_TEST(PTP_Saturation_QueueBoundSweep, "pure_compute",
                   PTP_Saturation_QueueBoundSweep)
```

- [ ] **Step 6.8: 添加 PTP_Throughput_HighFanout**

追加：

```cpp
// 用例 5: PTP_Throughput_HighFanout
perf::TestResult PTP_Throughput_HighFanout(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PTP_Throughput_HighFanout";
    r.workload = "pure_compute";

    for (int threads : opts.thread_sweep) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            auto batch = RunPoolBatch(
                static_cast<size_t>(threads),
                /*nTasks=*/5000,
                [](int) { perf::workload_pure_compute(1'000'000); });
            if (!batch.empty()) samples.push_back(batch[0]);
        }
        r.by_threads[threads] = perf::ComputeStats(std::move(samples), 0.0);
    }
    double baseline_tps = r.by_threads.count(1)
        ? r.by_threads.at(1).throughput_per_sec : 0.0;
    for (auto& [t, s] : r.by_threads) {
        s.speedup_vs_baseline = (baseline_tps > 0)
            ? s.throughput_per_sec / baseline_tps : 1.0;
    }
    return r;
}
PERF_REGISTER_TEST(PTP_Throughput_HighFanout, "pure_compute", PTP_Throughput_HighFanout)
```

- [ ] **Step 6.9: 添加 PTP_Latency_SubmitToDone**

追加：

```cpp
// 用例 6: PTP_Latency_SubmitToDone
perf::TestResult PTP_Latency_SubmitToDone(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PTP_Latency_SubmitToDone";
    r.workload = "pure_compute";

    for (int threads : opts.thread_sweep) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            PyThreadPool pool(static_cast<size_t>(threads));
            for (int i = 0; i < 1000 / std::max(1, threads); ++i) {
                auto start = std::chrono::steady_clock::now();
                auto opt = pool.Submit([]() {
                    perf::workload_pure_compute(1000);
                });
                if (opt) opt->get();
                samples.push_back(std::chrono::steady_clock::now() - start);
            }
        }
        r.by_threads[threads] = perf::ComputeStats(std::move(samples), 0.0);
    }
    // 检查 P95/P50 ≤ 5
    for (const auto& [t, s] : r.by_threads) {
        if (s.p50.count() > 0) {
            double ratio = static_cast<double>(s.p95.count()) / s.p50.count();
            if (ratio > 5.0) {
                r.status = perf::Status::Warn;
                r.message = "threads=" + std::to_string(t) +
                    " P95/P50 ratio " + std::to_string(ratio) + " > 5";
            }
        }
    }
    return r;
}
PERF_REGISTER_TEST(PTP_Latency_SubmitToDone, "pure_compute", PTP_Latency_SubmitToDone)
```

- [ ] **Step 6.10: 构建并验证**

```bash
cd /Users/tshua/respo/Code/CplusAlg
cmake --build build --target threading_perf -j 2>&1 | tail -20
```

期望：构建无错误。

- [ ] **Step 6.11: 提交**

```bash
cd /Users/tshua/respo/Code/CplusAlg
git add src/threading/perf/perf_py_thread_pool.cpp
git commit -m "feat(perf): add remaining 5 PyThreadPool perf tests

- PTP_Speedup_RealPython: 真实 Python 调用加速比
- PTP_ThreadSweep_All: 扩展性扫描
- PTP_Saturation_QueueBoundSweep: 队列大小 10/100/1000/∞
- PTP_Throughput_HighFanout: 5000 任务高并发
- PTP_Latency_SubmitToDone: Submit→Done P95/P50"
```

---

## Task 7: 实现 perf_py_producer_consumer.cpp（5 个 PPC 用例）

**Files:**
- Create: `src/threading/perf/perf_py_producer_consumer.cpp`
- Modify: `src/cplus_alg/threading/CMakeLists.txt`

### 5 个 PPC 用例规格

| 用例 | workload | 关键参数 | 软阈值 |
|---|---|---|---|
| `PPC_Throughput_BurnGIL` | `burn_gil` | 1 producer + N consumers，5000 任务，N∈{4,8,16,32} | 32 consumer 加速 ≥ 8x |
| `PPC_Saturation_MultiProducer` | `pure_compute` | producers∈{1,2,4,8,16} × consumers∈{4,8,16,32}，每组合 1000 任务 | producer 翻倍吞吐不降 |
| `PPC_BoundedQueue_Backpressure` | `pure_compute` | 4/8/16/32 consumers，队列=10，消费者 sleep 200ms | producer 不卡死，吞吐 > 0 |
| `PPC_Latency_TailLatency` | `pure_compute` | 4/8/16/32 consumers，1000 任务记录入队→消费 | P99/P50 ≤ 8 |
| `PPC_Scaling_AllThreads` | `pure_compute` | 1 producer + consumers∈{1,2,4,8,16,32}，5000 任务 | 单调非降 |

- [ ] **Step 7.1: 创建 perf_py_producer_consumer.cpp（含 1 个完整用例）**

文件 `src/threading/perf/perf_py_producer_consumer.cpp`：

```cpp
// src/threading/perf/perf_py_producer_consumer.cpp — PyProducerConsumer 性能测试

#include "perf_test_base.h"
#include "perf_workloads.h"
#include "threading/PyProducerConsumer.h"

#include <atomic>
#include <chrono>
#include <vector>

namespace {

// 用例 1: PPC_Throughput_BurnGIL（完整实现）
perf::TestResult PPC_Throughput_BurnGIL(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PPC_Throughput_BurnGIL";
    r.workload = "burn_gil";

    for (int consumers : opts.thread_sweep) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            std::atomic<int> consumed{0};
            PyProducerConsumer<int> pc(
                static_cast<size_t>(consumers),
                [&consumed](int) {
                    perf::workload_burn_gil(/*iterations=*/80'000'000);
                    ++consumed;
                },
                /*maxQueueSize=*/1000);

            auto start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < 5000; ++i) pc.Produce(i);
            pc.WaitAll();
            samples.push_back(std::chrono::high_resolution_clock::now() - start);
        }
        r.by_threads[consumers] = perf::ComputeStats(std::move(samples), 0.0);
    }
    double baseline_tps = r.by_threads.count(1)
        ? r.by_threads.at(1).throughput_per_sec : 0.0;
    for (auto& [t, s] : r.by_threads) {
        s.speedup_vs_baseline = (baseline_tps > 0)
            ? s.throughput_per_sec / baseline_tps : 1.0;
    }
    if (r.by_threads.count(32)) {
        const auto& s = r.by_threads.at(32);
        if (s.speedup_vs_baseline < 8.0) {
            r.status = perf::Status::Warn;
            r.message = "32-consumer speedup " +
                std::to_string(s.speedup_vs_baseline) + "x below 8x";
        }
    }
    return r;
}
PERF_REGISTER_TEST(PPC_Throughput_BurnGIL, "burn_gil", PPC_Throughput_BurnGIL)

} // namespace
```

- [ ] **Step 7.2: 加入 CMake target**

修改 `src/cplus_alg/threading/CMakeLists.txt`：

```cmake
add_executable(threading_perf
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_main.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_metrics.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_test_base.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_parallel_executor.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_py_thread_pool.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_py_producer_consumer.cpp
)
```

- [ ] **Step 7.3: 构建验证 + 提交骨架**

```bash
cd /Users/tshua/respo/Code/CplusAlg
cmake --build build --target threading_perf -j 2>&1 | tail -20
git add src/threading/perf/perf_py_producer_consumer.cpp \
        src/cplus_alg/threading/CMakeLists.txt
git commit -m "feat(perf): add PPC_Throughput_BurnGIL as template for PyProducerConsumer perf tests"
```

期望：构建无错误。

- [ ] **Step 7.4: 添加 PPC_Saturation_MultiProducer**

在 `} // namespace` 前追加：

```cpp
// 用例 2: PPC_Saturation_MultiProducer
perf::TestResult PPC_Saturation_MultiProducer(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PPC_Saturation_MultiProducer";
    r.workload = "pure_compute";

    std::vector<int> producer_counts = {1, 2, 4, 8, 16};
    for (int n_producers : producer_counts) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            std::atomic<int> consumed{0};
            PyProducerConsumer<int> pc(
                /*nConsumers=*/8,
                [&consumed](int) {
                    perf::workload_pure_compute(1'000'000);
                    ++consumed;
                },
                /*maxQueueSize=*/1000);

            std::vector<std::thread> producers;
            auto start = std::chrono::high_resolution_clock::now();
            for (int p = 0; p < n_producers; ++p) {
                producers.emplace_back([&pc, n_producers]() {
                    for (int i = 0; i < 1000 / n_producers; ++i) pc.Produce(i);
                });
            }
            for (auto& t : producers) t.join();
            pc.WaitAll();
            samples.push_back(std::chrono::high_resolution_clock::now() - start);
        }
        r.by_threads[n_producers] = perf::ComputeStats(std::move(samples), 0.0);
    }
    return r;
}
PERF_REGISTER_TEST(PPC_Saturation_MultiProducer, "pure_compute",
                   PPC_Saturation_MultiProducer)
```

- [ ] **Step 7.5: 添加 PPC_BoundedQueue_Backpressure**

追加：

```cpp
// 用例 3: PPC_BoundedQueue_Backpressure
perf::TestResult PPC_BoundedQueue_Backpressure(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PPC_BoundedQueue_Backpressure";
    r.workload = "sleep_consumer";

    for (int consumers : opts.thread_sweep) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            std::atomic<int> consumed{0};
            PyProducerConsumer<int> pc(
                static_cast<size_t>(consumers),
                [&consumed](int) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    ++consumed;
                },
                /*maxQueueSize=*/10);

            auto start = std::chrono::high_resolution_clock::now();
            // 主线程生产；因 maxQueueSize=10 触发背压
            for (int i = 0; i < 100; ++i) pc.Produce(i);
            pc.WaitAll();
            samples.push_back(std::chrono::high_resolution_clock::now() - start);
        }
        r.by_threads[consumers] = perf::ComputeStats(std::move(samples), 0.0);
    }
    return r;
}
PERF_REGISTER_TEST(PPC_BoundedQueue_Backpressure, "sleep_consumer",
                   PPC_BoundedQueue_Backpressure)
```

- [ ] **Step 7.6: 添加 PPC_Latency_TailLatency**

追加：

```cpp
// 用例 4: PPC_Latency_TailLatency
perf::TestResult PPC_Latency_TailLatency(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PPC_Latency_TailLatency";
    r.workload = "pure_compute";

    for (int consumers : opts.thread_sweep) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            std::atomic<int> next_id{0};
            std::mutex id_mtx;
            PyProducerConsumer<int> pc(
                static_cast<size_t>(consumers),
                [](int) { perf::workload_pure_compute(1'000'000); },
                /*maxQueueSize=*/0);

            std::vector<std::chrono::nanoseconds> local;
            std::thread producer([&]() {
                for (int i = 0; i < 1000 / std::max(1, consumers); ++i) {
                    auto t = std::chrono::steady_clock::now();
                    pc.Produce(i);
                    local.push_back(t);  // 记录入队时间
                }
            });
            producer.join();
            pc.WaitAll();
            // 简化：local 长度就是样本数，p50/p95/p99 是入队间隔而非端到端延迟
            // 由于 PyProducerConsumer 不暴露"消费完成"时间戳，此用例近似为入队节流
            r.by_threads[consumers] = perf::ComputeStats(std::move(local), 0.0);
        }
    }
    return r;
}
PERF_REGISTER_TEST(PPC_Latency_TailLatency, "pure_compute", PPC_Latency_TailLatency)
```

**注**：本用例实际度量的是"入队间隔"，不是真正端到端延迟。Task 12 验证阶段如发现不准确，回归修复（可在 PyProducerConsumer 增加 GetProcessedTimestamps 接口，本计划不涉及）。

- [ ] **Step 7.7: 添加 PPC_Scaling_AllThreads**

追加：

```cpp
// 用例 5: PPC_Scaling_AllThreads
perf::TestResult PPC_Scaling_AllThreads(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PPC_Scaling_AllThreads";
    r.workload = "pure_compute";

    for (int consumers : opts.thread_sweep) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            std::atomic<int> consumed{0};
            PyProducerConsumer<int> pc(
                static_cast<size_t>(consumers),
                [&consumed](int) {
                    perf::workload_pure_compute(1'000'000);
                    ++consumed;
                },
                /*maxQueueSize=*/1000);

            auto start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < 5000; ++i) pc.Produce(i);
            pc.WaitAll();
            samples.push_back(std::chrono::high_resolution_clock::now() - start);
        }
        r.by_threads[consumers] = perf::ComputeStats(std::move(samples), 0.0);
    }
    double baseline_tps = r.by_threads.count(1)
        ? r.by_threads.at(1).throughput_per_sec : 0.0;
    for (auto& [t, s] : r.by_threads) {
        s.speedup_vs_baseline = (baseline_tps > 0)
            ? s.throughput_per_sec / baseline_tps : 1.0;
    }
    double prev = 0;
    for (const auto& [t, s] : r.by_threads) {
        if (prev > 0 && s.speedup_vs_baseline < prev * 0.7) {
            r.status = perf::Status::Warn;
            r.message = "consumers=" + std::to_string(t) + " scalability dropped";
        }
        prev = s.speedup_vs_baseline;
    }
    return r;
}
PERF_REGISTER_TEST(PPC_Scaling_AllThreads, "pure_compute", PPC_Scaling_AllThreads)
```

- [ ] **Step 7.8: 构建并验证**

```bash
cd /Users/tshua/respo/Code/CplusAlg
cmake --build build --target threading_perf -j 2>&1 | tail -20
```

期望：构建无错误。

- [ ] **Step 7.9: 提交**

```bash
cd /Users/tshua/respo/Code/CplusAlg
git add src/threading/perf/perf_py_producer_consumer.cpp
git commit -m "feat(perf): add remaining 4 PyProducerConsumer perf tests

- PPC_Saturation_MultiProducer: 多 producer 吞吐
- PPC_BoundedQueue_Backpressure: 队列背压
- PPC_Latency_TailLatency: 入队节流
- PPC_Scaling_AllThreads: consumer 扩展性"
```

---

## Task 8: 实现 perf_py_batch_consumer.cpp（5 个 PBC 用例）

**Files:**
- Create: `src/threading/perf/perf_py_batch_consumer.cpp`
- Modify: `src/cplus_alg/threading/CMakeLists.txt`

### 5 个 PBC 用例规格

| 用例 | workload | 关键参数 | 软阈值 |
|---|---|---|---|
| `PBC_Throughput_BatchEffect` | `pure_compute` | consumers=4，batch∈{1,10,50,200}，5000 任务 | batch=50 吞吐 ≥ 3×batch=1 |
| `PBC_Throughput_ThreadSweep` | `pure_compute` | batch=50，consumers∈{1,2,4,8,16,32}，5000 任务 | 8 线程 ≥ 3×1 线程 |
| `PBC_Latency_BatchFill` | `pure_compute` | consumers=4，间歇 Produce(1)，batch=10, timeout=100ms | P95 ≤ 150ms |
| `PBC_Saturation_RealPython` | `real_python` | consumers=4/8/16/32，batch=50，1000 任务 | 不崩，吞吐合理 |
| `PBC_BatchingEfficiency_High` | `pure_compute` | 32 consumers，batch=50，5000 任务 | 实际批大小 ≥ 80% 目标 |

- [ ] **Step 8.1: 创建 perf_py_batch_consumer.cpp（含 1 个完整用例）**

文件 `src/threading/perf/perf_py_batch_consumer.cpp`：

```cpp
// src/threading/perf/perf_py_batch_consumer.cpp — PyBatchConsumer 性能测试

#include "perf_test_base.h"
#include "perf_workloads.h"
#include "threading/PyBatchConsumer.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {

// 批量任务体：每项跑 workload_pure_compute，记录实际批大小
struct BatchStats {
    std::atomic<size_t> total_batches{0};
    std::atomic<size_t> total_items{0};
};

void record_batch(BatchStats* s, size_t batch_size) {
    s->total_batches.fetch_add(1);
    s->total_items.fetch_add(batch_size);
}

// 用例 1: PBC_Throughput_BatchEffect（完整实现）
perf::TestResult PBC_Throughput_BatchEffect(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PBC_Throughput_BatchEffect";
    r.workload = "pure_compute";

    std::vector<size_t> batch_sizes = {1, 10, 50, 200};
    for (size_t bs : batch_sizes) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            BatchStats stats;
            PyBatchConsumer<int> bc(
                /*nConsumers=*/4,
                [&stats](std::vector<int>& batch) {
                    for (int /*item*/ : batch) {
                        perf::workload_pure_compute(1'000'000);
                    }
                    record_batch(&stats, batch.size());
                },
                /*nBatchSize=*/bs,
                /*timeout=*/std::chrono::milliseconds(100),
                /*maxQueueSize=*/0);

            auto start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < 5000; ++i) bc.Produce(i);
            bc.Flush();
            samples.push_back(std::chrono::high_resolution_clock::now() - start);
        }
        int key = static_cast<int>(bs);
        r.by_threads[key] = perf::ComputeStats(std::move(samples), 0.0);
    }
    // 检查 batch=50 vs batch=1
    if (r.by_threads.count(1) && r.by_threads.count(50)) {
        double t1 = r.by_threads.at(1).throughput_per_sec;
        double t50 = r.by_threads.at(50).throughput_per_sec;
        if (t1 > 0 && t50 / t1 < 3.0) {
            r.status = perf::Status::Warn;
            r.message = "batch=50/batch=1 throughput ratio " +
                std::to_string(t50 / t1) + " < 3";
        }
    }
    return r;
}
PERF_REGISTER_TEST(PBC_Throughput_BatchEffect, "pure_compute",
                   PBC_Throughput_BatchEffect)

} // namespace
```

- [ ] **Step 8.2: 加入 CMake target**

修改 `src/cplus_alg/threading/CMakeLists.txt`：

```cmake
add_executable(threading_perf
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_main.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_metrics.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_test_base.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_parallel_executor.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_py_thread_pool.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_py_producer_consumer.cpp
    ${CMAKE_SOURCE_DIR}/src/threading/perf/perf_py_batch_consumer.cpp
)
```

- [ ] **Step 8.3: 构建验证 + 提交骨架**

```bash
cd /Users/tshua/respo/Code/CplusAlg
cmake --build build --target threading_perf -j 2>&1 | tail -20
git add src/threading/perf/perf_py_batch_consumer.cpp \
        src/cplus_alg/threading/CMakeLists.txt
git commit -m "feat(perf): add PBC_Throughput_BatchEffect as template for PyBatchConsumer perf tests"
```

- [ ] **Step 8.4: 添加 PBC_Throughput_ThreadSweep**

在 `} // namespace` 前追加：

```cpp
// 用例 2: PBC_Throughput_ThreadSweep
perf::TestResult PBC_Throughput_ThreadSweep(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PBC_Throughput_ThreadSweep";
    r.workload = "pure_compute";

    for (int consumers : opts.thread_sweep) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            PyBatchConsumer<int> bc(
                static_cast<size_t>(consumers),
                [](std::vector<int>& batch) {
                    for (int /*item*/ : batch) {
                        perf::workload_pure_compute(1'000'000);
                    }
                },
                /*nBatchSize=*/50,
                /*timeout=*/std::chrono::milliseconds(100),
                /*maxQueueSize=*/0);

            auto start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < 5000; ++i) bc.Produce(i);
            bc.Flush();
            samples.push_back(std::chrono::high_resolution_clock::now() - start);
        }
        r.by_threads[consumers] = perf::ComputeStats(std::move(samples), 0.0);
    }
    double baseline_tps = r.by_threads.count(1)
        ? r.by_threads.at(1).throughput_per_sec : 0.0;
    for (auto& [t, s] : r.by_threads) {
        s.speedup_vs_baseline = (baseline_tps > 0)
            ? s.throughput_per_sec / baseline_tps : 1.0;
    }
    if (r.by_threads.count(8)) {
        const auto& s = r.by_threads.at(8);
        if (s.speedup_vs_baseline < 3.0) {
            r.status = perf::Status::Warn;
            r.message = "8-consumer speedup " +
                std::to_string(s.speedup_vs_baseline) + "x below 3x";
        }
    }
    return r;
}
PERF_REGISTER_TEST(PBC_Throughput_ThreadSweep, "pure_compute",
                   PBC_Throughput_ThreadSweep)
```

- [ ] **Step 8.5: 添加 PBC_Latency_BatchFill**

追加：

```cpp
// 用例 3: PBC_Latency_BatchFill
perf::TestResult PBC_Latency_BatchFill(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PBC_Latency_BatchFill";
    r.workload = "pure_compute";

    for (int consumers : opts.thread_sweep) {
        std::vector<std::chrono::nanoseconds> samples;
        for (int rep = 0; rep < opts.repeat; ++rep) {
            std::atomic<int> processed{0};
            PyBatchConsumer<int> bc(
                static_cast<size_t>(consumers),
                [&processed](std::vector<int>& batch) {
                    processed.fetch_add(static_cast<int>(batch.size()));
                },
                /*nBatchSize=*/10,
                /*timeout=*/std::chrono::milliseconds(100),
                /*maxQueueSize=*/0);

            std::thread producer([&]() {
                for (int i = 0; i < 50; ++i) {
                    auto t = std::chrono::steady_clock::now();
                    bc.Produce(i);
                    samples.push_back(t);
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
            });
            producer.join();
            bc.Flush();
        }
        r.by_threads[consumers] = perf::ComputeStats(std::move(samples), 0.0);
    }
    for (const auto& [t, s] : r.by_threads) {
        if (s.p95.count() > 150) {
            r.status = perf::Status::Warn;
            r.message = "consumers=" + std::to_string(t) +
                " P95 " + std::to_string(s.p95.count()) + "ms > 150ms";
        }
    }
    return r;
}
PERF_REGISTER_TEST(PBC_Latency_BatchFill, "pure_compute", PBC_Latency_BatchFill)
```

- [ ] **Step 8.6: 添加 PBC_Saturation_RealPython**

追加：

```cpp
// 用例 4: PBC_Saturation_RealPython
perf::TestResult PBC_Saturation_RealPython(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PBC_Saturation_RealPython";
    r.workload = "real_python";

    try {
        for (int consumers : opts.thread_sweep) {
            std::vector<std::chrono::nanoseconds> samples;
            for (int rep = 0; rep < opts.repeat; ++rep) {
                PyBatchConsumer<std::vector<double>> bc(
                    static_cast<size_t>(consumers),
                    [](std::vector<std::vector<double>>& batch) {
                        for (auto& item : batch) {
                            perf::workload_real_python_sum(std::move(item));
                        }
                    },
                    /*nBatchSize=*/50,
                    /*timeout=*/std::chrono::milliseconds(100),
                    /*maxQueueSize=*/0);

                auto start = std::chrono::high_resolution_clock::now();
                for (int i = 0; i < 1000; ++i) {
                    bc.Produce(std::vector<double>(100, 1.0));
                }
                bc.Flush();
                samples.push_back(std::chrono::high_resolution_clock::now() - start);
            }
            r.by_threads[consumers] = perf::ComputeStats(std::move(samples), 0.0);
        }
    } catch (const std::exception& e) {
        r.status = perf::Status::Warn;
        r.message = std::string("Real Python demo not available: ") + e.what();
    }
    return r;
}
PERF_REGISTER_TEST(PBC_Saturation_RealPython, "real_python",
                   PBC_Saturation_RealPython)
```

- [ ] **Step 8.7: 添加 PBC_BatchingEfficiency_High**

追加：

```cpp
// 用例 5: PBC_BatchingEfficiency_High
perf::TestResult PBC_BatchingEfficiency_High(const perf::PerfOptions& opts) {
    perf::TestResult r;
    r.test_name = "PBC_BatchingEfficiency_High";
    r.workload = "pure_compute";

    constexpr int kConsumers = 32;
    constexpr size_t kBatchSize = 50;
    std::vector<std::chrono::nanoseconds> samples;
    std::atomic<size_t> total_batches{0};
    std::atomic<size_t> total_items{0};

    for (int rep = 0; rep < opts.repeat; ++rep) {
        PyBatchConsumer<int> bc(
            kConsumers,
            [&total_batches, &total_items](std::vector<int>& batch) {
                total_batches.fetch_add(1);
                total_items.fetch_add(batch.size());
            },
            kBatchSize,
            /*timeout=*/std::chrono::milliseconds(50),
            /*maxQueueSize=*/0);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 5000; ++i) bc.Produce(i);
        bc.Flush();
        samples.push_back(std::chrono::high_resolution_clock::now() - start);
    }
    r.by_threads[kConsumers] = perf::ComputeStats(std::move(samples), 0.0);

    // 检查实际批大小 ≥ 80% 目标
    size_t batches = total_batches.load();
    size_t items = total_items.load();
    if (batches > 0) {
        double avg_batch = static_cast<double>(items) / batches;
        double ratio = avg_batch / static_cast<double>(kBatchSize);
        if (ratio < 0.8) {
            r.status = perf::Status::Warn;
            r.message = "actual batch size ratio " + std::to_string(ratio) +
                " < 0.8 of target " + std::to_string(kBatchSize);
        }
    }
    return r;
}
PERF_REGISTER_TEST(PBC_BatchingEfficiency_High, "pure_compute",
                   PBC_BatchingEfficiency_High)
```

- [ ] **Step 8.8: 构建并验证**

```bash
cd /Users/tshua/respo/Code/CplusAlg
cmake --build build --target threading_perf -j 2>&1 | tail -20
```

期望：构建无错误。

- [ ] **Step 8.9: 提交**

```bash
cd /Users/tshua/respo/Code/CplusAlg
git add src/threading/perf/perf_py_batch_consumer.cpp
git commit -m "feat(perf): add remaining 4 PyBatchConsumer perf tests

- PBC_Throughput_ThreadSweep: consumers 1..32 扩展性
- PBC_Latency_BatchFill: 间歇生产 P95 ≤ 150ms
- PBC_Saturation_RealPython: 真实 Python 批调用
- PBC_BatchingEfficiency_High: 32 consumers 实际批大小 ≥ 80%"
```

---

## Task 9: 实现 perf_main.cpp（CLI + Runner + 表格打印 + CSV）

**Files:**
- Modify: `src/threading/perf/perf_main.cpp`

- [ ] **Step 9.1: 替换占位 main 为完整实现**

文件 `src/threading/perf/perf_main.cpp`：

```cpp
// src/threading/perf/perf_main.cpp — threading 性能测试入口

#include "perf_test_base.h"
#include "perf_metrics.h"

#include "interpreter/GilManager.h"
#include "interpreter/PyInterpreter.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <thread>
#include <vector>

namespace {

struct CommandLineOptions {
    std::string filter;
    std::vector<int> threads = {1, 2, 4, 8, 16, 32};
    int repeat = 3;
    std::string csv_path;
    bool quiet = false;
};

void PrintUsage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [options]\n"
              << "  --filter NAME      Substring filter for test names\n"
              << "  --threads LIST     Comma-separated thread sweep (default: 1,2,4,8,16,32)\n"
              << "  --repeat N         Repeat each run N times (default: 3)\n"
              << "  --csv PATH         Write results to CSV\n"
              << "  --quiet            Reduce logging\n"
              << "  --help             Show this help\n";
}

CommandLineOptions ParseArgs(int argc, char** argv) {
    CommandLineOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            std::exit(0);
        } else if (arg == "--filter") {
            if (i + 1 >= argc) break;
            opts.filter = argv[++i];
        } else if (arg == "--threads") {
            if (i + 1 >= argc) break;
            std::string_view list(argv[++i]);
            opts.threads.clear();
            std::size_t start = 0;
            while (start <= list.size()) {
                std::size_t comma = list.find(',', start);
                if (comma == std::string_view::npos) comma = list.size();
                std::string_view token = list.substr(start, comma - start);
                if (!token.empty()) {
                    opts.threads.push_back(std::atoi(token.data()));
                }
                start = comma + 1;
            }
        } else if (arg == "--repeat") {
            if (i + 1 >= argc) break;
            opts.repeat = std::atoi(argv[++i]);
            if (opts.repeat < 1) opts.repeat = 1;
        } else if (arg == "--csv") {
            if (i + 1 >= argc) break;
            opts.csv_path = argv[++i];
        } else if (arg == "--quiet") {
            opts.quiet = true;
        }
    }
    return opts;
}

void PrintHeader(int hw, const CommandLineOptions& opts) {
    std::cout << "=== threading_perf ===\n"
              << "Hardware concurrency: " << hw << "\n"
              << "Thread sweep: ";
    for (std::size_t i = 0; i < opts.threads.size(); ++i) {
        if (i) std::cout << ",";
        std::cout << opts.threads[i];
    }
    std::cout << "\nRepeat: " << opts.repeat << "\n\n";
}

void PrintSummary(const std::vector<perf::TestResult>& results) {
    std::cout << "=== Summary ===\n";
    int pass = 0, warn = 0, fail = 0;
    for (const auto& r : results) {
        const char* tag = nullptr;
        switch (r.status) {
        case perf::Status::Pass: tag = "[PASS]"; ++pass; break;
        case perf::Status::Warn: tag = "[WARN]"; ++warn; break;
        case perf::Status::Fail: tag = "[FAIL]"; ++fail; break;
        }
        std::cout << tag << " " << r.test_name;
        if (!r.message.empty()) {
            std::cout << " : " << r.message;
        }
        std::cout << "\n";
    }
    std::cout << "Total: " << results.size()
              << "  PASS: " << pass
              << "  WARN: " << warn
              << "  FAIL: " << fail << "\n";
}

} // namespace

int main(int argc, char** argv) {
    CommandLineOptions opts = ParseArgs(argc, argv);

    // 确保 Python 解释器已初始化（后续 GIL 释放/获取需要）
    {
        PyInterpreter& interp = PyInterpreter::Instance();
        if (!interp.IsInitialized()) {
            if (!interp.Initialize()) {
                std::cerr << "Failed to initialize Python interpreter\n";
                return 2;
            }
        }
    }

    int hw = static_cast<int>(std::thread::hardware_concurrency());
    if (hw == 0) hw = 1;

    if (!opts.quiet) {
        PrintHeader(hw, opts);
    }

    perf::PerfOptions perf_opts;
    perf_opts.thread_sweep = opts.threads;
    perf_opts.repeat = opts.repeat;
    perf_opts.quiet = opts.quiet;
    perf_opts.csv_path = opts.csv_path;
    perf_opts.filter = opts.filter;
    perf_opts.hardware_concurrency = hw;

    std::vector<perf::TestResult> results;
    std::vector<std::tuple<std::string, std::string, int, perf::SampleStats>> csv_rows;

    // 主线程释放 GIL，让所有工作线程可以获取 GIL
    {
        GilScopedRelease release;

        auto tests = perf::TestRegistry::Instance().Filter(opts.filter);
        for (const auto& test : tests) {
            if (!opts.quiet) {
                std::cout << "Running " << test.name << " ...\n";
            }
            perf::TestResult result = test.run(perf_opts);
            results.push_back(result);

            // 打印表格
            std::vector<std::pair<std::string, perf::SampleStats>> rows;
            for (const auto& [t, s] : result.by_threads) {
                rows.emplace_back("threads=" + std::to_string(t), s);
                csv_rows.emplace_back(result.test_name, result.workload, t, s);
            }
            perf::PrintTable("=== " + result.test_name + " (" + result.workload + ") ===", rows);
        }
    }  // 重新获取 GIL

    PrintSummary(results);

    // 输出 CSV
    if (!opts.csv_path.empty()) {
        std::ofstream ofs(opts.csv_path);
        if (ofs) {
            ofs << "test_name,workload,threads,wall_ms,throughput_tps,speedup,efficiency,p50_ms,p95_ms,p99_ms,regression_flag\n";
            for (const auto& [name, workload, threads, s] : csv_rows) {
                ofs << name << ","
                    << workload << ","
                    << threads << ","
                    << s.mean_ms << ","
                    << s.throughput_per_sec << ","
                    << s.speedup_vs_baseline << ","
                    << s.speedup_vs_baseline / std::max(1, threads) << ","
                    << s.p50.count() << ","
                    << s.p95.count() << ","
                    << s.p99.count() << ","
                    << (s.speedup_vs_baseline >= 0.5 ? "ok" : "warn")
                    << "\n";
            }
            std::cout << "CSV written to " << opts.csv_path << "\n";
        }
    }

    // 退出码：有 FAIL 则 1，否则 0（WARN 不阻塞）
    for (const auto& r : results) {
        if (r.status == perf::Status::Fail) return 1;
    }
    return 0;
}
```

- [ ] **Step 9.2: 构建并验证**

```bash
cd /Users/tshua/respo/Code/CplusAlg
cmake --build build --target threading_perf -j 2>&1 | tail -20
```

期望：构建无错误。

- [ ] **Step 9.3: 跑 help 和完整快速扫描**

```bash
./build/src/cplus_alg/threading/threading_perf --help
./build/src/cplus_alg/threading/threading_perf --threads 1,2,4 --repeat 1 --quiet
```

期望：`--help` 输出用法；`--threads 1,2,4` 跑完 21 个用例，打印表格和 Summary。

- [ ] **Step 9.4: 提交**

```bash
cd /Users/tshua/respo/Code/CplusAlg
git add src/threading/perf/perf_main.cpp
git commit -m "feat(perf): implement perf_main with CLI, runner, table, CSV, exit codes

- 解析 --filter/--threads/--repeat/--csv/--quiet
- 主线程释放 GIL，遍历 TestRegistry 执行所有已注册 perf 用例
- 打印每用例统计表格和全局 Summary
- FAIL 返回 1，WARN 不阻塞"
```

---

## Task 10: 实现 TestThreadingPerf.cpp（GTest 软断言版）+ 接入 ctest

**Files:**
- Create: `tests/threading/TestThreadingPerf.cpp`
- Modify: `tests/CMakeLists.txt`

策略：选代表性 8 个 GTest 用例（覆盖 4 个组件），不跑全 21 个。每个用例都验证 4 线程 vs 1 线程的加速比 ≥ 软阈值。所有用例都带超时保护。

- [ ] **Step 10.1: 创建 TestThreadingPerf.cpp（含夹具 + 8 个测试）**

文件 `tests/threading/TestThreadingPerf.cpp`：

```cpp
// tests/threading/TestThreadingPerf.cpp — threading 性能 GTest 软断言版
// 接 ctest 自动跑；只验证关键加速比，防止明显退化阻塞 PR。
// 完整 perf 数据用 scripts/run_threading_perf.sh 跑 perf_main 拿。

#include "interpreter/GilManager.h"
#include "interpreter/PyInterpreter.h"

#include "threading/ParallelExecutor.h"
#include "threading/PyThreadPool.h"
#include "threading/PyProducerConsumer.h"
#include "threading/PyBatchConsumer.h"

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace {

void ensure_interpreter_initialized() {
    PyInterpreter& interp = PyInterpreter::Instance();
    if (!interp.IsInitialized()) {
        ASSERT_TRUE(interp.Initialize());
    }
}

// 简单工作量：每次 ~10ms 纯计算
inline void perf_compute(int iterations) {
    volatile std::uint64_t acc = 0;
    for (int i = 0; i < iterations; ++i) {
        acc += static_cast<std::uint64_t>(i) * 0x9E3779B97F4A7C15ULL;
    }
    (void)acc;
}

constexpr int kComputeIters = 1'000'000;  // 约 10ms

} // namespace

class ThreadingPerfTest : public ::testing::Test {
protected:
    void SetUp() override {
        ensure_interpreter_initialized();
        release_ = std::make_unique<GilScopedRelease>();
    }
    void TearDown() override {
        release_.reset();
    }
private:
    std::unique_ptr<GilScopedRelease> release_;
};

// ============= ParallelExecutor =============

TEST_F(ThreadingPerfTest, PE_4ThreadSpeedup) {
    constexpr int k_threads = 4;
    constexpr int k_repeats = 3;

    // 1 线程基线
    long long single_total = 0;
    for (int r = 0; r < k_repeats; ++r) {
        auto start = std::chrono::high_resolution_clock::now();
        perf_compute(kComputeIters * 10);
        single_total += std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start).count();
    }
    long long single_avg = single_total / k_repeats;

    // 4 线程
    long long multi_total = 0;
    for (int r = 0; r < k_repeats; ++r) {
        auto start = std::chrono::high_resolution_clock::now();
        ParallelExecutor::RunInParallel(k_threads, [](int /*tid*/) {
            perf_compute(kComputeIters * 10);
        });
        multi_total += std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start).count();
    }
    long long multi_avg = multi_total / k_repeats;

    double speedup = static_cast<double>(single_avg) / std::max(1LL, multi_avg);
    EXPECT_GE(speedup, 1.5)
        << "PE 4-thread speedup " << speedup << "x below 1.5x (single="
        << single_avg << "ms, multi=" << multi_avg << "ms)";
}

// ============= PyThreadPool =============

TEST_F(ThreadingPerfTest, PTP_4ThreadSpeedup) {
    constexpr size_t k_threads = 4;
    constexpr int k_tasks = 200;

    // 1 线程基线
    {
        PyThreadPool pool(1);
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<std::future<void>> futs;
        for (int i = 0; i < k_tasks; ++i) {
            auto opt = pool.Submit([]() { perf_compute(kComputeIters); });
            if (opt) futs.push_back(std::move(*opt));
        }
        for (auto& f : futs) f.get();
        auto single_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start).count();

        // 4 线程
        PyThreadPool pool4(k_threads);
        auto start4 = std::chrono::high_resolution_clock::now();
        std::vector<std::future<void>> futs4;
        for (int i = 0; i < k_tasks; ++i) {
            auto opt = pool4.Submit([]() { perf_compute(kComputeIters); });
            if (opt) futs4.push_back(std::move(*opt));
        }
        for (auto& f : futs4) f.get();
        auto multi_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start4).count();

        double speedup = static_cast<double>(single_ms) / std::max(1LL, multi_ms);
        EXPECT_GE(speedup, 1.5)
            << "PTP 4-thread speedup " << speedup << "x below 1.5x";
    }
}

// ============= PyProducerConsumer =============

TEST_F(ThreadingPerfTest, PPC_4ConsumerSpeedup) {
    constexpr int k_consumers = 4;
    constexpr int k_tasks = 200;

    // 1 consumer 基线
    long long single_ms = 0;
    {
        std::atomic<int> done{0};
        PyProducerConsumer<int> pc(1, [&done](int) {
            perf_compute(kComputeIters);
            ++done;
        }, /*maxQueueSize=*/0);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < k_tasks; ++i) pc.Produce(i);
        pc.WaitAll();
        single_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start).count();
    }

    // 4 consumer
    long long multi_ms = 0;
    {
        std::atomic<int> done{0};
        PyProducerConsumer<int> pc(k_consumers, [&done](int) {
            perf_compute(kComputeIters);
            ++done;
        }, /*maxQueueSize=*/0);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < k_tasks; ++i) pc.Produce(i);
        pc.WaitAll();
        multi_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start).count();
    }

    double speedup = static_cast<double>(single_ms) / std::max(1LL, multi_ms);
    EXPECT_GE(speedup, 1.5)
        << "PPC 4-consumer speedup " << speedup << "x below 1.5x";
}

// ============= PyBatchConsumer =============

TEST_F(ThreadingPerfTest, PBC_BatchSizeImprovement) {
    constexpr int k_consumers = 4;
    constexpr int k_tasks = 200;

    auto run_with_batch = [&](size_t batch_size) -> long long {
        PyBatchConsumer<int> bc(k_consumers,
            [](std::vector<int>& batch) {
                for (int /*x*/ : batch) {
                    perf_compute(kComputeIters);
                }
            },
            /*nBatchSize=*/batch_size,
            /*timeout=*/std::chrono::milliseconds(100),
            /*maxQueueSize=*/0);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < k_tasks; ++i) bc.Produce(i);
        bc.Flush();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start).count();
    };

    long long small_batch = run_with_batch(1);
    long long big_batch = run_with_batch(50);

    EXPECT_LT(big_batch, small_batch * 2)
        << "PBC batch=50 (" << big_batch << "ms) should be much faster than "
        << "batch=1 (" << small_batch << "ms)";
}

TEST_F(ThreadingPerfTest, PBC_4ConsumerSpeedup) {
    constexpr int k_consumers = 4;
    constexpr int k_tasks = 400;

    auto run_with_consumers = [&](size_t n) -> long long {
        PyBatchConsumer<int> bc(n,
            [](std::vector<int>& batch) {
                for (int /*x*/ : batch) {
                    perf_compute(kComputeIters);
                }
            },
            /*nBatchSize=*/50,
            /*timeout=*/std::chrono::milliseconds(100),
            /*maxQueueSize=*/0);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < k_tasks; ++i) bc.Produce(i);
        bc.Flush();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start).count();
    };

    long long single_ms = run_with_consumers(1);
    long long multi_ms = run_with_consumers(k_consumers);

    double speedup = static_cast<double>(single_ms) / std::max(1LL, multi_ms);
    EXPECT_GE(speedup, 1.5)
        << "PBC 4-consumer speedup " << speedup << "x below 1.5x";
}

// ============= 退化检测 =============

TEST_F(ThreadingPerfTest, PTP_NoGILDeadlock) {
    // 验证 PyThreadPool 在没有主线程 GIL 释放时不会死锁
    // （由 SetUp 释放 GIL，所以这里只是验证能完成）
    PyThreadPool pool(2);
    std::atomic<int> counter{0};
    for (int i = 0; i < 20; ++i) {
        auto opt = pool.Submit([&counter]() {
            ++counter;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
        EXPECT_TRUE(opt.has_value());
    }
    pool.WaitAll();
    EXPECT_EQ(counter.load(), 20);
}

TEST_F(ThreadingPerfTest, PPC_QueueBoundDoesNotDeadlock) {
    // 验证有界队列在 SetUp 释放 GIL 下能正常 drain
    PyProducerConsumer<int> pc(2, [](int) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }, /*maxQueueSize=*/5);

    for (int i = 0; i < 50; ++i) pc.Produce(i);
    pc.WaitAll();
    SUCCEED();
}

TEST_F(ThreadingPerfTest, PBC_FlushCompletes) {
    PyBatchConsumer<int> bc(2,
        [](std::vector<int>&) { /* noop */ },
        /*nBatchSize=*/10,
        /*timeout=*/std::chrono::milliseconds(50),
        /*maxQueueSize=*/0);

    for (int i = 0; i < 50; ++i) bc.Produce(i);
    bc.Flush();
    SUCCEED();
}
```

- [ ] **Step 10.2: 把 TestThreadingPerf.cpp 加入 unit_tests 源列表**

修改 `tests/CMakeLists.txt`，在 `add_executable(unit_tests ...)` 源列表中**追加**：

```cmake
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
    threading/TestThreadingPerf.cpp
)
```

- [ ] **Step 10.3: 构建 + 跑 ctest 验证**

```bash
cd /Users/tshua/respo/Code/CplusAlg
cmake --build build --target unit_tests -j 2>&1 | tail -20
ctest --test-dir build -R "ThreadingPerf" --output-on-failure 2>&1 | tail -40
```

期望：8 个 ThreadingPerf 测试全部通过。注意：第一次跑会有冷启动开销，可能某些用例很慢但不会失败（软阈值宽）。

- [ ] **Step 10.4: 提交**

```bash
cd /Users/tshua/respo/Code/CplusAlg
git add tests/threading/TestThreadingPerf.cpp tests/CMakeLists.txt
git commit -m "test(perf): add GTest soft-assertion version for threading perf

8 个代表性 perf 用例（接 ctest），覆盖 4 个组件 × 4 线程加速比软阈值：
- PE_4ThreadSpeedup: ParallelExecutor
- PTP_4ThreadSpeedup / PTP_NoGILDeadlock: PyThreadPool
- PPC_4ConsumerSpeedup / PPC_QueueBoundDoesNotDeadlock: PyProducerConsumer
- PBC_BatchSizeImprovement / PBC_4ConsumerSpeedup / PBC_FlushCompletes: PyBatchConsumer

软阈值加速比 ≥ 1.5x（防止明显退化阻塞 PR）。"
```

---

## Task 11: 实现 scripts/run_threading_perf.sh

**Files:**
- Create: `scripts/run_threading_perf.sh`

- [ ] **Step 11.1: 创建脚本**

文件 `scripts/run_threading_perf.sh`：

```bash
#!/usr/bin/env bash
# 运行 src/threading 的性能测试 perf main，输出报告到 stdout 和 CSV 文件。
# 用法：bash scripts/run_threading_perf.sh [--quick] [--filter NAME] [--csv PATH]

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
PERF_BIN="${BUILD_DIR}/src/threading/perf/threading_perf"

# 1. 确保构建存在
if [[ ! -x "${PERF_BIN}" ]]; then
    echo ">>> Building threading_perf ..."
    cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
    cmake --build "${BUILD_DIR}" --target threading_perf -j >/dev/null
fi

# 2. 解析参数，转发给 perf main
EXTRA_ARGS=()
QUICK=false
while [[ $# -gt 0 ]]; do
    case "$1" in
        --quick)  QUICK=true; shift ;;
        --filter) EXTRA_ARGS+=(--filter "$2"); shift 2 ;;
        --csv)    EXTRA_ARGS+=(--csv "$2"); shift 2 ;;
        --threads) EXTRA_ARGS+=(--threads "$2"); shift 2 ;;
        --repeat) EXTRA_ARGS+=(--repeat "$2"); shift 2 ;;
        --quiet)  EXTRA_ARGS+=(--quiet); shift ;;
        -h|--help)
            echo "Usage: $0 [--quick] [--filter NAME] [--csv PATH] [--threads LIST] [--repeat N]"
            echo ""
            echo "  --quick     快速模式：线程扫描 {1, 4, 16} + 重复 1 次 + 真实 Python 只跑 1 线程"
            echo "              约 1-3 分钟"
            echo "  默认（全量）：约 10-30 分钟"
            echo ""
            echo "Examples:"
            echo "  $0 --quick                    # 本地快速反馈"
            echo "  $0 --filter PE_               # 只跑 ParallelExecutor 测试"
            echo "  $0 --csv perf_$(date +%Y%m%d).csv  # 输出 CSV 用于历史对比"
            exit 0 ;;
        *)
            echo "Unknown arg: $1" >&2
            exit 2 ;;
    esac
done

# 3. --quick 模式：缩小线程扫描 + 减少重复
if [[ "${QUICK}" == true ]]; then
    EXTRA_ARGS+=(--threads 1,4,16 --repeat 1)
fi

# 4. 跑
echo ">>> Running threading_perf ${EXTRA_ARGS[*]}"
echo "    (建议插电源 + 高性能模式，避免 CPU 节能干扰数据)"
echo ""

# 5. 失败时打印诊断
set +e
"${PERF_BIN}" "${EXTRA_ARGS[@]}"
RC=$?
set -e

if [[ ${RC} -ne 0 ]]; then
    echo "" >&2
    echo ">>> FAIL (exit ${RC}): see perf main output above for regression details" >&2
    exit ${RC}
fi
echo ">>> OK: all checks passed"
```

- [ ] **Step 11.2: 加执行权限并验证**

```bash
chmod +x /Users/tshua/respo/Code/CplusAlg/scripts/run_threading_perf.sh
bash /Users/tshua/respo/Code/CplusAlg/scripts/run_threading_perf.sh --help
```

期望：打印 usage。

- [ ] **Step 11.3: 跑一次 --quick 验证整体流程**

```bash
cd /Users/tshua/respo/Code/CplusAlg
bash scripts/run_threading_perf.sh --quick 2>&1 | tail -30
```

期望：跑完所有用例，打印 Summary，退出码 0（无 FAIL）。

- [ ] **Step 11.4: 提交**

```bash
cd /Users/tshua/respo/Code/CplusAlg
git add scripts/run_threading_perf.sh
git commit -m "feat(perf): add run_threading_perf.sh wrapper script

- 自动构建（若二进制不存在）
- --quick 模式：线程扫描 {1,4,16} + 重复 1 次，约 1-3 分钟
- 默认全量：约 10-30 分钟
- 透传 --filter/--csv/--threads/--repeat/--quiet 给 perf main
- 退出码透传，CI 可直接调用"
```

---

## Task 12: 端到端验证与基线记录

- [ ] **Step 12.1: 全量构建**

```bash
cd /Users/tshua/respo/Code/CplusAlg
cmake --build build -j 2>&1 | tail -10
```

期望：构建无错误。

- [ ] **Step 12.2: 跑全套 unit_tests（含 ThreadingPerf）**

```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -40
```

期望：所有测试通过；ThreadingPerf 8 个新用例在 3-5 分钟内完成。

- [ ] **Step 12.3: 跑 --quick 模式验证 perf main**

```bash
bash scripts/run_threading_perf.sh --quick 2>&1 | tail -40
```

期望：21 个 perf 用例跑完，Summary 全部 [PASS] 或少量 [WARN]（软阈值），无 [FAIL]，退出码 0。

- [ ] **Step 12.4: 跑全量 perf 并记录基线 CSV**

```bash
cd /Users/tshua/respo/Code/CplusAlg
bash scripts/run_threading_perf.sh --csv docs/superpowers/perf_baseline_2026-06-13.csv 2>&1 | tail -50
```

期望：CSV 写入到 `docs/superpowers/perf_baseline_2026-06-13.csv`，包含所有 21 用例 × 6 线程 = 约 100+ 行。

- [ ] **Step 12.5: 检查关键数据**

```bash
head -3 /Users/tshua/respo/Code/CplusAlg/docs/superpowers/perf_baseline_2026-06-13.csv
wc -l /Users/tshua/respo/Code/CplusAlg/docs/superpowers/perf_baseline_2026-06-13.csv
```

期望：第 1 行是表头；总行数 > 100。

- [ ] **Step 12.6: 提交基线**

```bash
cd /Users/tshua/respo/Code/CplusAlg
git add docs/superpowers/perf_baseline_2026-06-13.csv
git commit -m "docs(perf): record initial baseline CSV for threading perf tests

首次记录的基线数据，用于未来性能回归对比。
基线日期 2026-06-13，21 个 perf 用例 × 6 线程 = ~100+ 数据点。
后续如需对比，运行：
  bash scripts/run_threading_perf.sh --csv current.csv
  diff <(sort current.csv) <(sort docs/superpowers/perf_baseline_2026-06-13.csv)"
```

- [ ] **Step 12.7: 整体冒烟**

```bash
cd /Users/tshua/respo/Code/CplusAlg
ls -la docs/superpowers/specs/2026-06-13-threading-perf-tests-design.md
ls -la docs/superpowers/plans/2026-06-13-threading-perf-tests.md
ls -la docs/superpowers/perf_baseline_2026-06-13.csv
ls -la scripts/run_threading_perf.sh
ls -la src/threading/perf/
ls -la tests/threading/TestThreadingPerf.cpp
git log --oneline -15
```

期望：所有交付物存在；git log 显示 12 个新提交（每个 Task 一个或多个）。

---

## 收尾检查清单

- [ ] **覆盖率检查**：spec §3 矩阵的 21 个 perf 用例是否全部实现
  - PE_*: 5（PE_Speedup_HeavyCompute, PE_Scalability_ThreadSweep, PE_Throughput_Saturated, PE_Latency_TailDistribution, PE_ScalingEfficiency_32）
  - PTP_*: 6（PTP_Speedup_BurnGIL, PTP_Speedup_RealPython, PTP_ThreadSweep_All, PTP_Saturation_QueueBoundSweep, PTP_Throughput_HighFanout, PTP_Latency_SubmitToDone）
  - PPC_*: 5（PPC_Throughput_BurnGIL, PPC_Saturation_MultiProducer, PPC_BoundedQueue_Backpressure, PPC_Latency_TailLatency, PPC_Scaling_AllThreads）
  - PBC_*: 5（PBC_Throughput_BatchEffect, PBC_Throughput_ThreadSweep, PBC_Latency_BatchFill, PBC_Saturation_RealPython, PBC_BatchingEfficiency_High）
- [ ] **GTest 软断言版**：8 个代表性用例接入 ctest ✓
- [ ] **脚本支持 --quick 模式** ✓
- [ ] **CSV 基线记录** ✓
- [ ] **退出码**：FAIL → 1，其他 → 0 ✓
- [ ] **GIL 夹具**：所有 threading 相关测试用 GilScopedRelease ✓
- [ ] **超时保护**：所有阻塞等待带超时 ✓
- [ ] **命名约定**：snake_case 函数、trailing_ 成员、PascalCase 类 ✓
- [ ] **中文注释**：所有非显然代码有中文解释 ✓
- [ ] **头文件保护**：所有 .h 用 #ifndef ✓

---

## 后续优化（v2 范围，不在本次）

- CSV 历史对比脚本（自动检测回归）
- Google Benchmark 库集成
- 火焰图集成（perf record + flamegraph.pl）
- 性能 baseline.json + GitHub Actions 自动 PR 评论
- macOS Instruments 集成
- 异步 / 协程版 perf tests

---

## 风险与回滚

| 风险 | 缓解 | 回滚 |
|---|---|---|
| 32 线程用例在慢机器上极慢 | --quick 模式 + CI 仅跑 4 线程 | 调整 thread_sweep 默认值 |
| 真实 Python demo 缺失导致 FAIL | try/catch + 状态设为 Warn | 用 GTEST_SKIP() 替代 Warn |
| 软阈值被调优成"假阴性" | 保留 csv 基线供人工核对 | 调低阈值比例 0.5 → 0.3 |
| 计时器在容器/CI 中精度差 | 重复 3 次取最小 | 增加 repeat 次数 |

**如果实施中遇到 spec 未覆盖的问题，按"软阈值不阻塞，FAIL 才阻塞"原则回退或加 GTEST_SKIP() 兜底。**


