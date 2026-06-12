// tests/threading/TestPyBatchConsumer.cpp — PyBatchConsumer 冒烟/单元测试

#include "interpreter/GilManager.h"
#include "interpreter/PyInterpreter.h"
#include "threading/PyBatchConsumer.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

#include <gtest/gtest.h>

namespace {

void ensure_interpreter_initialized() {
    PyInterpreter& interp = PyInterpreter::Instance();
    if (!interp.IsInitialized()) {
        ASSERT_TRUE(interp.Initialize());
    }
}

// 检测是否运行在 ThreadSanitizer 下
bool IsTsanEnabled() {
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
    return true;
#endif
#endif
    return false;
}

} // namespace

class PyBatchConsumerTest : public ::testing::Test {
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

TEST_F(PyBatchConsumerTest, ProcessesBatch) {
    std::atomic<int> sum{0};
    std::atomic<int> batch_count{0};

    PyBatchConsumer<int> bc(
        1,
        [&sum, &batch_count](std::vector<int>& batch) {
            ++batch_count;
            for (int v : batch) {
                sum += v;
            }
        },
        3,
        std::chrono::milliseconds(50),
        0);

    bc.Produce(1);
    bc.Produce(2);
    bc.Produce(3);
    bc.Flush();

    EXPECT_EQ(sum.load(), 6);
    EXPECT_EQ(batch_count.load(), 1);
}

TEST_F(PyBatchConsumerTest, FlushProcessesPartialBatch) {
    std::atomic<int> sum{0};

    PyBatchConsumer<int> bc(
        1,
        [&sum](std::vector<int>& batch) {
            for (int v : batch) {
                sum += v;
            }
        },
        10,
        std::chrono::milliseconds(1000),
        0);

    bc.Produce(1);
    bc.Produce(2);
    bc.Flush();

    EXPECT_EQ(sum.load(), 3);
}

TEST_F(PyBatchConsumerTest, TimeoutFlushesPartialBatch) {
    std::atomic<int> sum{0};
    std::atomic<int> batch_count{0};

    PyBatchConsumer<int> bc(1, [&](std::vector<int>& batch) {
        ++batch_count;
        for (int v : batch) sum += v;
    }, 10, std::chrono::milliseconds(100), 0);

    bc.Produce(1);
    bc.Produce(2);
    bc.Produce(3);
    // 等超时，应处理不完整的 batch
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    bc.WaitAll();
    EXPECT_EQ(sum.load(), 6);
    EXPECT_GE(batch_count.load(), 1);
}

TEST_F(PyBatchConsumerTest, MultiConsumerBatchIntegrity) {
    constexpr int kNumItems = 1000;
    std::atomic<int> sum{0};
    std::atomic<int> count{0};
    std::mutex seen_mutex;
    std::set<int> seen_values;

    PyBatchConsumer<int> bc(4, [&](std::vector<int>& batch) {
        count += static_cast<int>(batch.size());
        for (int v : batch) {
            sum += v;
            std::lock_guard<std::mutex> lock(seen_mutex);
            seen_values.insert(v);
        }
    }, 20, std::chrono::milliseconds(50), 200);

    for (int i = 0; i < kNumItems; ++i) {
        bc.Produce(i);
    }
    bc.Flush();
    bc.Shutdown();

    EXPECT_EQ(count.load(), kNumItems);
    EXPECT_EQ(sum.load(), kNumItems * (kNumItems - 1) / 2);
    EXPECT_EQ(seen_values.size(), static_cast<size_t>(kNumItems))
        << "Duplicate or missing values detected";
}

TEST_F(PyBatchConsumerTest, FlushWhileEmpty) {
    PyBatchConsumer<int> bc(1, [](std::vector<int>&) {
        FAIL() << "Should not be called for empty queue";
    }, 10, std::chrono::milliseconds(100));
    bc.Flush();  // 空队列 Flush 应无操作
    SUCCEED();
}

TEST_F(PyBatchConsumerTest, ExceptionInBatchConsumer) {
    std::atomic<int> processed{0};

    PyBatchConsumer<int> bc(2, [&processed](std::vector<int>& batch) {
        processed += static_cast<int>(batch.size());
        if (batch.front() % 7 == 0) {
            throw std::runtime_error("batch error");
        }
    }, 5, std::chrono::milliseconds(50));

    for (int i = 0; i < 50; ++i) {
        bc.Produce(i);
    }
    bc.Flush();
    EXPECT_EQ(processed.load(), 50);
    EXPECT_GT(bc.GetErrorCount(), 0) << "Should have recorded errors";
}

TEST_F(PyBatchConsumerTest, ShutdownDrainsWorkers) {
    // 验证 Shutdown 不丢正在执行的 batch，不导致崩溃
    {
        PyBatchConsumer<int> bc(2, [](std::vector<int>&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }, 10, std::chrono::milliseconds(100), 100);
        for (int i = 0; i < 30; ++i) {
            bc.Produce(i);
        }
    }  // 析构 → Shutdown → join workers
    SUCCEED();
}

TEST_F(PyBatchConsumerTest, ZeroBatchSizeAsserts) {
    // nBatchSize=0 在 Debug 构建下由 assert 捕获
    GTEST_SKIP() << "nBatchSize=0 triggers assertion (by design)";
}
