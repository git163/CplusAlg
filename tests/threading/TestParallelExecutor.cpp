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
