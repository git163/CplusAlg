#include "interpreter/GilManager.h"
#include "interpreter/PyInterpreter.h"
#include <pybind11/embed.h>
#include <gtest/gtest.h>

class GilManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(PyInterpreter::Instance().Initialize());
    }
};

TEST_F(GilManagerTest, ReleaseThenAcquireRestoresGIL) {
    {
        GilScopedRelease release;
    }
    // 释放后 GIL 恢复，可安全调用 Python API
    py::gil_scoped_acquire gil;
    py::module_ sys = py::module_::import("sys");
    EXPECT_GT(py::len(sys.attr("version")), 0);
}

TEST_F(GilManagerTest, AcquireAfterRelease) {
    // 释放 GIL 后重新获取，验证可安全调用 Python API
    {
        GilScopedRelease release;
        // 此时无 GIL
        {
            GilScopedAcquire acquire;
            // GIL 已恢复，可调用 Python API
            py::module_ sys = py::module_::import("sys");
            EXPECT_GT(py::len(sys.attr("version")), 0);
        }
    }
}
