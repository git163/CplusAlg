// tests/interpreter/TestPyInterpreter.cpp — PyInterpreter 冒烟/单元测试（含 sys.path 去重验证）

#include "interpreter/PyInterpreter.h"
#include "cplus_alg/python/python_backend.h"

#include <pybind11/embed.h>

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

namespace py = pybind11;

// PyInterpreter 是进程级单例，且 CTest 通过 gtest_discover_tests 让每个 TEST 单独进程运行，
// 因此每个测试都必须独立保证解释器处于期望的初始状态。

TEST(PyInterpreter, InitializeSuccessfully) {
    PyInterpreter& interp = PyInterpreter::Instance();
    if (!interp.IsInitialized()) {
        EXPECT_TRUE(interp.Initialize());
    }
    EXPECT_TRUE(interp.IsInitialized());
}

TEST(PyInterpreter, InitializeIsIdempotent) {
    PyInterpreter& interp = PyInterpreter::Instance();
    ASSERT_TRUE(interp.Initialize());

    EXPECT_TRUE(interp.Initialize());
    EXPECT_TRUE(interp.IsInitialized());
}

TEST(PyInterpreter, ExtraSysPathIsApplied) {
    PyInterpreter& interp = PyInterpreter::Instance();
    ASSERT_TRUE(interp.Initialize());

    const std::string k_dummy_path = "/tmp/cplusalg_pyinterp_test_path";
    EXPECT_TRUE(interp.Initialize({k_dummy_path}));

    py::gil_scoped_acquire gil;
    py::module_ sys = py::module_::import("sys");
    py::list path_list = sys.attr("path");

    bool found = false;
    for (const auto& item : path_list) {
        std::string p = item.cast<std::string>();
        if (p == k_dummy_path) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "extra sys.path was not applied";
}

TEST(PyInterpreter, ExtraSysPathIsNotDuplicated) {
    PyInterpreter& interp = PyInterpreter::Instance();
    ASSERT_TRUE(interp.Initialize());

    const std::string k_dummy_path = "/tmp/cplusalg_pyinterp_unique_test_path";
    EXPECT_TRUE(interp.Initialize({k_dummy_path}));
    EXPECT_TRUE(interp.Initialize({k_dummy_path}));

    py::gil_scoped_acquire gil;
    py::module_ sys = py::module_::import("sys");
    py::list path_list = sys.attr("path");

    int count = 0;
    for (const auto& item : path_list) {
        try {
            std::string p = item.cast<std::string>();
            if (p == k_dummy_path) {
                ++count;
            }
        } catch (...) {
            continue;
        }
    }
    EXPECT_EQ(count, 1) << "extra sys.path was duplicated";
}

TEST(PyInterpreter, FinalizeResetsState) {
    PyInterpreter& interp = PyInterpreter::Instance();
    ASSERT_TRUE(interp.Initialize());

    interp.Finalize();
    EXPECT_FALSE(interp.IsInitialized());
}

TEST(PyInterpreter, FinalizeInvalidatesPythonBackend) {
    PyInterpreter& interp = PyInterpreter::Instance();
    ASSERT_TRUE(interp.Initialize());

    cplus_alg::python::python_backend backend;
    EXPECT_TRUE(backend.available());

    interp.Finalize();
    EXPECT_FALSE(backend.available());
}

TEST(PyInterpreter, ReinitAfterFinalize) {
    PyInterpreter& interp = PyInterpreter::Instance();
    ASSERT_TRUE(interp.Initialize());
    interp.Finalize();
    ASSERT_FALSE(interp.IsInitialized());

    // 重新初始化
    EXPECT_TRUE(interp.Initialize());
    EXPECT_TRUE(interp.IsInitialized());

    // 验证 sys.path 仍然正确设置
    py::gil_scoped_acquire gil;
    py::module_ sys = py::module_::import("sys");
    EXPECT_GE(py::len(sys.attr("path")), 1);
}

TEST(PyInterpreter, ConcurrentInitFinalizeCycle) {
    // CPython 限制：Py_Initialize 在 Py_Finalize 后的重入 + 多线程组合
    // 存在无法避免的死锁（GIL 转移与线程状态重建冲突）
    GTEST_SKIP() << "CPython limitation: re-init after Finalize + multi-thread deadlocks (GIL/thread-state conflict)";
}
