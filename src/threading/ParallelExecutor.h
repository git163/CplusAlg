#pragma once

#include "interpreter/GilManager.h"
#include <spdlog/spdlog.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <utility>
#include <vector>

// 并行执行工具
// 提供多线程并行执行函数、迭代执行、基准测试对比等通用能力
// 所有方法自动处理 GIL 释放/获取
// 工作线程内部自动捕获异常并记录日志，避免未处理异常导致程序崩溃
namespace ParallelExecutor {

// 在 nThreads 个线程中并行执行 func(thread_id)
// 主线程自动释放 GIL，工作线程内部自动获取 GIL
inline void RunInParallel(int nThreads, std::function<void(int)> func) {
    GilScopedRelease release;

    std::vector<std::thread> vecThreads;
    for (int t = 0; t < nThreads; ++t) {
        vecThreads.emplace_back([t, &func]() {
            try {
                GilScopedAcquire gil;
                func(t);
            } catch (const std::exception& e) {
                spdlog::error("RunInParallel worker {} failed: {}", t, e.what());
            }
        });
    }

    for (auto& t : vecThreads) {
        t.join();
    }
}

// 在 nThreads 个线程中并行执行 func(thread_id, iteration)
// 每个线程迭代 nIterations 次
// 主线程自动释放 GIL，工作线程内部每次迭代自动获取 GIL
inline void RunInParallelIterated(int nThreads,
                                   int nIterations,
                                   std::function<void(int, int)> func) {
    GilScopedRelease release;

    std::vector<std::thread> vecThreads;
    for (int t = 0; t < nThreads; ++t) {
        vecThreads.emplace_back([t, nIterations, &func]() {
            for (int i = 0; i < nIterations; ++i) {
                try {
                    GilScopedAcquire gil;
                    func(t, i);
                } catch (const std::exception& e) {
                    spdlog::error("RunInParallelIterated worker {} iteration {} failed: {}",
                                  t, i, e.what());
                }
            }
        });
    }

    for (auto& t : vecThreads) {
        t.join();
    }
}

// 执行单线程 vs 多线程基准测试对比
// 单线程：顺序调用 func(i) i=0..nCalls-1
// 多线程：nThreads 个线程，线程 t 调用 func(t), func(t+nThreads), ...
// 多线程模式下自动在主线程释放 GIL，func 需自行管理 GIL
// 返回 {singleMs, multiMs}
inline std::pair<long long, long long> RunBenchmarkComparison(
    int nCalls,
    int nThreads,
    std::function<void(int)> func) {
    // 单线程顺序执行
    auto startSingle = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < nCalls; ++i) {
        func(i);
    }
    auto endSingle = std::chrono::high_resolution_clock::now();
    auto singleMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        endSingle - startSingle).count();

    // 多线程并发执行
    auto startMulti = std::chrono::high_resolution_clock::now();
    {
        GilScopedRelease release;

        std::vector<std::thread> vecThreads;
        for (int t = 0; t < nThreads; ++t) {
            vecThreads.emplace_back([t, nCalls, nThreads, &func]() {
                for (int i = t; i < nCalls; i += nThreads) {
                    try {
                        func(i);
                    } catch (const std::exception& e) {
                        spdlog::error("RunBenchmarkComparison worker {} call {} failed: {}",
                                      t, i, e.what());
                    }
                }
            });
        }

        for (auto& t : vecThreads) {
            t.join();
        }
    }
    auto endMulti = std::chrono::high_resolution_clock::now();
    auto multiMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        endMulti - startMulti).count();

    return {singleMs, multiMs};
}

} // namespace ParallelExecutor
