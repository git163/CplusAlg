// tests/threading/TestPyBatchConsumer.cpp — PyBatchConsumer 冒烟/单元测试

#include "interpreter/GilManager.h"
#include "interpreter/PyInterpreter.h"
#include "threading/PyBatchConsumer.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

namespace {

void ensure_interpreter_initialized() {
    PyInterpreter& interp = PyInterpreter::Instance();
    if (!interp.IsInitialized()) {
        ASSERT_TRUE(interp.Initialize());
    }
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
