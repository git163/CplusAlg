// tests/threading/TestParallelExecutor.cpp — ParallelExecutor 冒烟/单元测试

#include "interpreter/GilManager.h"
#include "interpreter/PyInterpreter.h"
#include "threading/ParallelExecutor.h"

#include <atomic>
#include <chrono>
#include <memory>

#include <gtest/gtest.h>

namespace {

void ensure_interpreter_initialized() {
    PyInterpreter& interp = PyInterpreter::Instance();
    if (!interp.IsInitialized()) {
        ASSERT_TRUE(interp.Initialize());
    }
}

} // namespace

// ParallelExecutor 内部已自行管理 GIL 释放/获取，测试夹具只需确保解释器已初始化。
class ParallelExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        ensure_interpreter_initialized();
    }
};

TEST_F(ParallelExecutorTest, RunInParallel) {
    constexpr int k_num_threads = 4;
    std::atomic<int> counter{0};
    ParallelExecutor::RunInParallel(k_num_threads, [&counter](int /*tid*/) {
        ++counter;
    });
    EXPECT_EQ(counter.load(), k_num_threads);
}

TEST_F(ParallelExecutorTest, RunInParallelIterated) {
    constexpr int k_num_threads = 3;
    constexpr int k_iterations = 5;
    std::atomic<int> counter{0};
    ParallelExecutor::RunInParallelIterated(
        k_num_threads, k_iterations, [&counter](int /*tid*/, int /*iter*/) {
            ++counter;
        });
    EXPECT_EQ(counter.load(), k_num_threads * k_iterations);
}

TEST_F(ParallelExecutorTest, RunBenchmarkComparison) {
    constexpr int k_num_calls = 10;
    constexpr int k_num_threads = 2;
    std::atomic<int> counter{0};
    auto [single_ms, multi_ms] = ParallelExecutor::RunBenchmarkComparison(
        k_num_calls, k_num_threads, [&counter](int /*idx*/) {
            ++counter;
        });
    // RunBenchmarkComparison 会先单线程执行一次，再多线程执行一次
    EXPECT_EQ(counter.load(), k_num_calls * 2);
    EXPECT_GE(single_ms, 0);
    EXPECT_GE(multi_ms, 0);
}

TEST_F(ParallelExecutorTest, ExceptionInWorkerDoesNotCrash) {
    std::atomic<int> call_count{0};
    ParallelExecutor::RunInParallel(4, [&](int thread_id) {
        ++call_count;
        if (thread_id == 2) {
            throw std::runtime_error("worker error");
        }
    });
    EXPECT_EQ(call_count.load(), 4);
}

TEST_F(ParallelExecutorTest, BenchmarkShowsPositiveTiming) {
    auto [single_ms, multi_ms] = ParallelExecutor::RunBenchmarkComparison(
        1000, 4, [](int) {
            volatile int x = 0;
            for (int i = 0; i < 10000; ++i) x += i;
        });
    // 不做严格加速比断言，但应都为正
    EXPECT_GT(single_ms, 0);
    EXPECT_GT(multi_ms, 0);
}

TEST_F(ParallelExecutorTest, ZeroThreads) {
    // nThreads=0 行为：不应调用 callback
    std::atomic<int> count{0};
    ParallelExecutor::RunInParallel(0, [&](int) { ++count; });
    EXPECT_EQ(count.load(), 0);
}

TEST_F(ParallelExecutorTest, HighIterationCount) {
    constexpr int kThreads = 4;
    constexpr int kIterations = 1000;
    std::atomic<long long> total{0};

    ParallelExecutor::RunInParallelIterated(kThreads, kIterations,
        [&](int thread_id, int iter) {
            total += static_cast<long long>(thread_id) * kIterations + iter;
        });

    // 验证总次数正确
    long long expected = 0;
    for (int t = 0; t < kThreads; ++t) {
        for (int i = 0; i < kIterations; ++i) {
            expected += static_cast<long long>(t) * kIterations + i;
        }
    }
    EXPECT_EQ(total.load(), expected);
}
