// tests/threading/TestPyProducerConsumer.cpp — PyProducerConsumer 冒烟/单元测试

#include "interpreter/GilManager.h"
#include "interpreter/PyInterpreter.h"
#include "threading/PyProducerConsumer.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace {

void ensure_interpreter_initialized() {
    PyInterpreter& interp = PyInterpreter::Instance();
    if (!interp.IsInitialized()) {
        ASSERT_TRUE(interp.Initialize());
    }
}

constexpr std::chrono::seconds k_wait_timeout{5};

} // namespace

class PyProducerConsumerTest : public ::testing::Test {
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

TEST_F(PyProducerConsumerTest, ProcessesSingleTask) {
    PyProducerConsumer<int> pc(1, [](const int& v) { (void)v; });
    pc.Produce(42);
    pc.WaitAll();
    EXPECT_EQ(pc.GetProcessedCount(), 1u);
}

TEST_F(PyProducerConsumerTest, ProcessesMultipleTasks) {
    constexpr int k_num_tasks = 10;
    std::atomic<int> sum{0};
    PyProducerConsumer<int> pc(2, [&sum](const int& v) { sum += v; });

    for (int i = 1; i <= k_num_tasks; ++i) {
        pc.Produce(i);
    }

    pc.WaitAll();
    EXPECT_EQ(pc.GetProcessedCount(), static_cast<size_t>(k_num_tasks));
    EXPECT_EQ(sum.load(), 55);
}

TEST_F(PyProducerConsumerTest, TryProduceTimesOutWhenQueueFull) {
    // consumer 处理每个任务耗时较长，期间再生产一个填满队列，
    // 第三次 TryProduce 应因队列满而超时。
    PyProducerConsumer<int> pc(1, [](const int& v) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        (void)v;
    }, 1);

    pc.Produce(1);
    pc.Produce(2);

    bool produced = pc.TryProduce(std::chrono::milliseconds(50), 3);
    EXPECT_FALSE(produced);

    pc.WaitAll();
}

TEST_F(PyProducerConsumerTest, ProduceBatch) {
    PyProducerConsumer<int> pc(2, [](const int& v) { (void)v; });
    std::vector<int> items = {1, 2, 3, 4, 5};
    pc.ProduceBatch(items.begin(), items.end());
    pc.WaitAll();
    EXPECT_EQ(pc.GetProcessedCount(), items.size());
}

TEST_F(PyProducerConsumerTest, ErrorCountIncrementsOnException) {
    PyProducerConsumer<int> pc(1, [](const int& /*v*/) {
        throw std::runtime_error("expected error");
    });
    pc.Produce(1);
    pc.WaitAll();
    EXPECT_EQ(pc.GetErrorCount(), 1u);
    EXPECT_EQ(pc.GetProcessedCount(), 1u);
}
