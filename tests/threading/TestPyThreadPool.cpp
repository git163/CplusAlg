// tests/threading/TestPyThreadPool.cpp — PyThreadPool 冒烟/单元测试
// 所有阻塞等待都带超时，避免死锁导致 CI 卡死。

#include "interpreter/GilManager.h"
#include "interpreter/PyInterpreter.h"
#include "threading/PyThreadPool.h"

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

namespace {

// 确保解释器已初始化；PyThreadPool 工作线程需要获取 GIL。
void ensure_interpreter_initialized() {
    PyInterpreter& interp = PyInterpreter::Instance();
    if (!interp.IsInitialized()) {
        ASSERT_TRUE(interp.Initialize());
    }
}

constexpr std::chrono::seconds k_wait_timeout{5};

} // namespace

// 测试夹具：确保解释器已初始化，并在测试期间释放主线程 GIL，
// 避免 PyThreadPool 工作线程因拿不到 GIL 而死锁。
class PyThreadPoolTest : public ::testing::Test {
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

TEST_F(PyThreadPoolTest, SubmitSingleTask) {
    PyThreadPool pool(1);
    auto opt_fut = pool.Submit([]() { return 42; });
    ASSERT_TRUE(opt_fut.has_value());

    const auto status = opt_fut->wait_for(k_wait_timeout);
    ASSERT_NE(status, std::future_status::timeout)
        << "thread pool task timed out (possible deadlock)";
    EXPECT_EQ(opt_fut->get(), 42);
}

TEST_F(PyThreadPoolTest, SubmitMultipleTasks) {
    constexpr int k_num_tasks = 10;
    PyThreadPool pool(2);
    std::vector<std::future<int>> futures;
    futures.reserve(k_num_tasks);

    for (int i = 0; i < k_num_tasks; ++i) {
        auto opt_fut = pool.Submit([i]() { return i * i; });
        ASSERT_TRUE(opt_fut.has_value());
        futures.push_back(std::move(*opt_fut));
    }

    for (int i = 0; i < k_num_tasks; ++i) {
        const auto status = futures[i].wait_for(k_wait_timeout);
        ASSERT_NE(status, std::future_status::timeout)
            << "task " << i << " timed out (possible deadlock)";
        EXPECT_EQ(futures[i].get(), i * i);
    }
}

TEST_F(PyThreadPoolTest, WaitAllWaitsForPendingTasks) {
    PyThreadPool pool(2);
    std::atomic<int> counter{0};
    for (int i = 0; i < 4; ++i) {
        auto opt_fut = pool.Submit([&counter]() {
            ++counter;
            return 0;
        });
        EXPECT_TRUE(opt_fut.has_value());
    }

    pool.WaitAll();
    EXPECT_EQ(counter.load(), 4);
}

TEST_F(PyThreadPoolTest, ShutdownRejectsNewTasks) {
    PyThreadPool pool(1);
    auto opt_fut1 = pool.Submit([]() { return 1; });
    ASSERT_TRUE(opt_fut1.has_value());

    pool.Shutdown();
    auto opt_fut2 = pool.Submit([]() { return 2; });
    EXPECT_FALSE(opt_fut2.has_value());
}

TEST_F(PyThreadPoolTest, BoundedQueueBlocksProducer) {
    constexpr size_t k_max_queue_size = 2;
    PyThreadPool pool(1, k_max_queue_size);

    std::atomic<int> completed{0};
    std::vector<std::future<int>> futures;
    futures.reserve(k_max_queue_size + 1);

    // 先提交超过队列容量的任务；第三个会阻塞，直到消费者取走一个
    for (int i = 0; i < static_cast<int>(k_max_queue_size + 1); ++i) {
        auto opt_fut = pool.Submit([&completed]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            ++completed;
            return 0;
        });
        ASSERT_TRUE(opt_fut.has_value());
        futures.push_back(std::move(*opt_fut));
    }

    // 等待所有任务完成，限时防止死锁
    for (auto& fut : futures) {
        const auto status = fut.wait_for(k_wait_timeout);
        ASSERT_NE(status, std::future_status::timeout)
            << "bounded queue test timed out (possible deadlock)";
        fut.get();
    }

    EXPECT_EQ(completed.load(), static_cast<int>(k_max_queue_size + 1));
}
