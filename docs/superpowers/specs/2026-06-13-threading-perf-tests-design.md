# src/threading 模块多线程效率测试套件设计

**日期**：2026-06-13
**作者**：brainstorming 会话
**状态**：待用户审阅

## 1. 背景与目标

`src/threading` 模块包含 4 个组件：`ParallelExecutor`、`PyThreadPool`、`PyProducerConsumer<T>`、`PyBatchConsumer<T>`。现有 `tests/threading/` 中已有 4 个 GTest 文件覆盖**正确性**，但**没有任何效率/性能测试**——无法检测线程池串行化、GIL 死锁、扩展性退化等隐性回归。

**目标**：为这 4 个组件补一套**效率测试**，覆盖：
- 加速比 / 扩展性曲线（4/8/16/32 线程）
- 饱和度（负载 vs 资源）
- 延迟 / 响应时间（P50/P95/P99 尾延迟）
- 吞吐量压测（tasks/sec）

## 2. 总体架构（方案 C：混合）

两套测试，职责清晰：

| 入口 | 位置 | 触发 | 断言 | 输出 |
|---|---|---|---|---|
| **GTest 软断言版** | `tests/threading/TestThreadingPerf.cpp` | `ctest` 自动 | 软阈值（加速比 ≥ 0.5×理论值） | GTest 报告 |
| **Perf main** | `src/threading/perf/perf_main.cpp` | 手动 `bash scripts/run_threading_perf.sh` | 退出码 0/非 0 | 人类可读表格 + CSV |

**为什么分开**：
- GTest 接入 CI（自动），但不适合"绝对毫秒"类断言（慢 CI 假阳性）
- Perf main 不接 CI（perf 用例 10-30 分钟，不能阻塞 PR），但提供完整报告便于人眼/历史对比

**目录布局**：

```
src/threading/
├── perf/                                  # 新增：性能测试专用子目录
│   ├── perf_main.cpp                      # 独立 main()，打印完整基准报告
│   ├── perf_workloads.h                   # 三类 workload（纯 C++ / GIL 模拟 / 真实 Python）
│   ├── perf_metrics.h                     # 计时、统计、打印辅助
│   ├── perf_parallel_executor.cpp         # ParallelExecutor 性能测试
│   ├── perf_py_thread_pool.cpp            # PyThreadPool 性能测试
│   ├── perf_py_producer_consumer.cpp      # PyProducerConsumer 性能测试
│   └── perf_py_batch_consumer.cpp         # PyBatchConsumer 性能测试
└── CMakeLists.txt                         # 新增 threading_perf 可执行文件

tests/threading/
└── TestThreadingPerf.cpp                  # 新增：GTest 软断言版（接 ctest）

scripts/
└── run_threading_perf.sh                  # 新增：调用 perf main
```

`tests/` 目录按 CLAUDE.md 约定为"单元测试"（接 ctest），perf main 不属于单元测试范畴。放在 `src/threading/perf/` 保持"生产代码 + 自带基准工具"边界清晰。

## 3. 测试矩阵（21 个 perf 用例）

### 3.1 `ParallelExecutor`（5 个）

| 用例 | 指标 | Workload | 线程数 | 软阈值 |
|---|---|---|---|---|
| `PE_Speedup_HeavyCompute` | 加速比 | 纯 C++ 计算（~100ms/块） | 4/8/16/32 vs 1 | 加速比 ≥ 0.7×理论值 |
| `PE_Scalability_ThreadSweep` | 扩展性曲线 | 固定总工作量 | {1,2,4,8,16,32} | 单调非降（弱） |
| `PE_Throughput_Saturated` | 吞吐 | 短任务 × 100000 | 4/8/16/32 | 8 线程 ≥ 4× 1 线程 |
| `PE_Latency_TailDistribution` | 尾延迟 | 1000 任务 | 4/8/16/32 | P99/P50 ≤ 10 |
| `PE_ScalingEfficiency_32` | 32 线程效率 | 大工作量 | 32 vs 1 | 效率 ≥ 50% |

### 3.2 `PyThreadPool`（6 个）

| 用例 | 指标 | Workload | 线程数 | 软阈值 |
|---|---|---|---|---|
| `PTP_Speedup_BurnGIL` | 加速比 | `GilScopedRelease + 80ms 计算` | 4/8/16/32 vs 1 | 32 线程加速比 ≥ 8x |
| `PTP_Speedup_RealPython` | 加速比 | 真实 numpy_ops sleep 变体 | 4/8/16/32 | 加速比 ≥ 0.5×理论 |
| `PTP_ThreadSweep_All` | 扩展性 | 5000 任务，纯 C++ | {1,2,4,8,16,32} | 单调非降（弱） |
| `PTP_Saturation_QueueBoundSweep` | 饱和度 | 队列 10/100/1000/∞ | 4/8/16/32 | 吞吐不显著下降 |
| `PTP_Throughput_HighFanout` | 吞吐 | 4/8/16/32 × 5000 任务 | 同 | ≥ 1.5× 1 线程 |
| `PTP_Latency_SubmitToDone` | 端到端延迟 | 1000 任务 | 4/8/16/32 | P95 ≤ 5×P50 |

### 3.3 `PyProducerConsumer<T>`（5 个）

| 用例 | 指标 | Workload | 线程数 | 软阈值 |
|---|---|---|---|---|
| `PPC_Throughput_BurnGIL` | 吞吐 | 1 producer + N consumers，5000 任务 | N = 4/8/16/32 | 32 consumer 加速 ≥ 8x |
| `PPC_Saturation_MultiProducer` | 饱和度 | 1/2/4/8/16 producers + 4/8/16/32 consumers | 二维 | producer 翻倍不降低吞吐 |
| `PPC_BoundedQueue_Backpressure` | 背压 | 队列=10，消费者 sleep 200ms | 4/8/16/32 | producer 不卡死 |
| `PPC_Latency_TailLatency` | 尾延迟 | 1000 任务 | 4/8/16/32 | P99/P50 ≤ 8 |
| `PPC_Scaling_AllThreads` | 全线程扫描 | 5000 任务，纯 C++ | {1,2,4,8,16,32} | 单调非降 |

### 3.4 `PyBatchConsumer<T>`（5 个）

| 用例 | 指标 | Workload | 线程数 | 软阈值 |
|---|---|---|---|---|
| `PBC_Throughput_BatchEffect` | 吞吐 vs 批大小 | batch=1/10/50/200 | 4/8/16/32 | batch=50 吞吐 ≥ 3×batch=1 |
| `PBC_Throughput_ThreadSweep` | 扩展性 | 5000 任务，batch=50 | {1,2,4,8,16,32} | 8 线程 ≥ 3×1 线程 |
| `PBC_Latency_BatchFill` | 批填充延迟 | 间歇 Produce(1) | 4/8/16/32 | P95 ≤ 150ms |
| `PBC_Saturation_RealPython` | 真实 Python 批调用 | numpy batch 变体 | 4/8/16/32 | 不崩，吞吐合理 |
| `PBC_BatchingEfficiency_High` | 高并发下批效率 | 32 consumers，batch=50 | 32 | 实际批大小 ≥ 80% 目标 |

每个 perf 用例都有**两个版本**：GTest 软断言版（`tests/`）和 perf main 详细报告版（`src/threading/perf/`）。GTest 只跑代表性 workload/线程数，perf main 跑全套。

## 4. Workload 设计

放在 `src/threading/perf/perf_workloads.h`。

### 4.1 Workload A：纯 C++ 计算

```cpp
// 测出理论加速比上限，验证基础设施
inline void workload_pure_compute(int iterations) {
    volatile std::uint64_t acc = 0;
    for (int i = 0; i < iterations; ++i) {
        acc += static_cast<std::uint64_t>(i) * 0x9E3779B97F4A7C15ULL;
    }
    (void)acc;
}
```
- 每次调用 ~100ms 纯计算，cache miss / branch 干扰少
- 不取 GIL，扩展性应接近线性

### 4.2 Workload B：GIL 模拟

```cpp
// 模拟"Python 释放 GIL 后做 CPU 密集"的最常见生产模式
inline std::int64_t workload_burn_gil(int iterations) {
    GilScopedRelease release;          // 关键：模拟 numpy 的 GIL 释放
    volatile std::uint64_t acc = 0;
    for (int i = 0; i < iterations; ++i) {
        acc += static_cast<std::uint64_t>(i) * 0x9E3779B97F4A7C15ULL;
    }
    return static_cast<std::int64_t>(acc);
}
```
- 对应 numpy/pandas 等 C 扩展在多线程下的真实行为
- 加速比上界 ≈ 核数（GIL 不再成瓶颈）

### 4.3 Workload C：真实 Python 调用

```cpp
// 端到端验证，捕获 Python ↔ C++ 转换开销
inline double workload_real_python_sum(std::vector<double>&& data) {
    PyModule caller("demo.numpy_ops");
    return caller.Call("sum_array", data).cast<double>();
}
```
- 复用 `tests/python/` 已有 demo 模块
- 数据规模可调（小数组 ~1ms，大数组 ~50ms）
- demo 模块未编译时 `GTEST_SKIP()` 兜底

## 5. 软阈值标准

原则：**慢 CI 机器能过，但明显串行化/GIL 死锁会挂**。

| 阈值类型 | 公式 | 理由 |
|---|---|---|
| **加速比下限** | `≥ 0.5 × 理论加速比`（理论 = `min(线程数, 核数)`） | 4 核机器 8 线程加速比 ≥ 2x；32 核 32 线程 ≥ 8x |
| **扩展性单调性** | `accel(N) ≥ accel(N/2) × 0.7` | 允许小幅抖动（OS 调度），不允许断崖下降 |
| **尾延迟比** | `P99/P50 ≤ 10`（无锁）/ `≤ 8`（有锁） | 软上限，长尾说明争用或饥饿 |
| **效率下限（高线程）** | `32 线程效率 ≥ 0.5` | 防止退化成"32 个 1 线程" |
| **吞吐下限** | 绝对值软下限（如"≥ 1000 tasks/s"） | 仅作参考，主要看趋势 |

## 6. 统计与计时（`perf_metrics.h`）

```cpp
namespace perf {

struct SampleStats {
    std::chrono::milliseconds p50, p95, p99, max;
    double mean_ms;
    double throughput_per_sec;
    double speedup_vs_baseline;
};

SampleStats ComputeStats(std::vector<std::chrono::nanoseconds>&& samples);
void PrintTable(const std::string& title,
                const std::vector<std::pair<std::string, SampleStats>>& rows);
void PrintCsv(const std::string& path,
              const std::vector<std::pair<std::string, SampleStats>>& rows);
}
```

**计时原则**：
- 加速比测试：`std::chrono::high_resolution_clock` 外层包裹
- 延迟测试：`std::chrono::steady_clock`（不受系统时间影响）
- 吞吐测试：`wall_clock` 总耗时 / 任务数
- **每个用例重复 3-5 次取中位数**（消除冷启动、CPU 频率波动）

## 7. Perf Main 设计

### 7.1 CLI

```
./threading_perf [--filter NAME] [--threads 1,2,4,8,16,32] [--repeat N] [--csv PATH] [--quiet]
```

### 7.2 输出格式

主表（每个用例一组）：

```
=== PyThreadPool Speedup (workload_burn_gil, 5000 tasks) ===
threads | wall_ms | throughput | speedup | efficiency | P50 | P95 | P99
--------|---------|------------|---------|------------|-----|-----|-----
   1    |  4023   |  1243 tps  |  1.00x  |    -       |   0 |   0 |   0
   2    |  2102   |  2379 tps  |  1.91x  |   95.7%    |   0 |   0 |   0
   ...
  32    |   341   | 14663 tps  | 11.79x  |   36.8%    |   0 |   0 |   0
Hardware concurrency: 10  (theoretical max speedup: 10.00x)
```

末页汇总（带退出码决定）：

```
=== Summary ===
[PASS] PE_Speedup_HeavyCompute    : 4-thread speedup=3.42x  (>= 2.00x)
[WARN] PBC_Throughput_BatchEffect : batch=50 vs batch=1 = 2.1x (expected >= 3.0x)
[FAIL] PPC_Saturation_MultiProducer : 16-producer throughput 15% lower
Total: 21  PASS: 18  WARN: 2  FAIL: 1
```

### 7.3 退出码

| 状态 | 退出码 | 含义 |
|---|---|---|
| 全 PASS | 0 | 性能无退化 |
| 有 WARN | 0 | 警告不阻塞 |
| 有 FAIL | 1 | 触发硬阈值失败 |

### 7.4 CSV 输出

```csv
test_name,workload,threads,wall_ms,throughput_tps,speedup,efficiency,p50_ms,p95_ms,p99_ms,regression_flag
PE_Speedup_HeavyCompute,pure_compute,1,4023,248,1.00,1.000,0,0,0,ok
...
```

## 8. 脚本与构建

### 8.1 `scripts/run_threading_perf.sh`

- 自动构建（如果二进制不存在）
- `--quick` 模式：线程扫描 `{1, 4, 16}` + 重复 1 次 + 跳过真实 Python，约 1-3 分钟
- 默认全量：约 10-30 分钟
- `--csv` 输出到文件

### 8.2 CMake 集成（`src/threading/CMakeLists.txt`）

```cmake
add_executable(threading_perf
    perf/perf_main.cpp
    perf/perf_parallel_executor.cpp
    perf/perf_py_thread_pool.cpp
    perf/perf_py_producer_consumer.cpp
    perf/perf_py_batch_consumer.cpp
)
target_link_libraries(threading_perf PRIVATE cplus_alg_lib spdlog::spdlog GTest::gtest)
target_include_directories(threading_perf PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_compile_features(threading_perf PRIVATE cxx_std_17)
```

**关键决策**：
- **不**通过 `gtest_discover_tests` 注册（perf 用例太长，不阻塞 CI）
- **不**通过 `add_test()` 接入 ctest
- 仅构建为可执行文件，靠脚本手动触发

### 8.3 GTest 软断言版（`tests/threading/TestThreadingPerf.cpp`）

- 接入 ctest（`gtest_discover_tests` 自动发现）
- `tests/CMakeLists.txt` 加超时保护 `set_tests_properties(... PROPERTIES TIMEOUT 600)`
- 每个 GTest 用 `GilScopedRelease` 夹具（与现有约定一致）

## 9. 风险与缓解

| 风险 | 缓解 |
|---|---|
| GTest 软断言假阳性（慢 CI 机器） | 阈值用"相对值"（≥ 0.5×理论），不假设绝对毫秒 |
| GTest 软断言假阴性（快机器） | 阈值不设上限 |
| 真实 Python demo 模块缺失 | `GTEST_SKIP()` 兜底（与现有约定一致） |
| 32 线程 CI 跑爆 CPU | CI GTest 软断言版只跑 4 线程代表用例；32 线程仅手动 perf |
| spdlog 在 perf main 中刷屏 | `--quiet` 降低日志级别，只打表格 |
| CPU 节能模式影响数据 | 脚本提示"建议插电源 + 高性能模式"，perf main 启动打印 `current_governor` |
| PyInterpreter 单例与多测试冲突 | 每个测试 fixture 重新 `Initialize`/`Finalize`（沿用现有模式） |

## 10. 时间预算

| 场景 | 时长 | 用途 |
|---|---|---|
| GTest 软断言版（CI 自动） | 3-5 min | 防止明显退化阻塞 PR |
| Perf main `--quick`（本地） | 1-3 min | 开发期快速反馈 |
| Perf main 全量（手动） | 10-30 min | 性能调优、记录基线 |

## 11. 不做的事（YAGNI）

- 真实基线 CSV 历史对比工具（v2 扩展）
- Google Benchmark 库集成（避免新依赖）
- 多进程 perf / 分布式
- 火焰图 / perf.data 集成
- 性能 baseline.json 自动 PR 注释
- 协程 / async 版本
- PyTorch / TensorRT 集成测试

## 12. 实现顺序建议

1. `perf_workloads.h` + `perf_metrics.h`（基础设施）
2. `perf_parallel_executor.cpp`（最简单，先打通流程）
3. `perf_py_thread_pool.cpp`（核心组件）
4. `perf_py_producer_consumer.cpp`
5. `perf_py_batch_consumer.cpp`
6. `perf_main.cpp`（注册 + CLI + 打印）
7. `scripts/run_threading_perf.sh`
8. `tests/threading/TestThreadingPerf.cpp` + CMake
9. 本地跑通全量 + `--quick`
10. CI 验证 GTest 软断言版
