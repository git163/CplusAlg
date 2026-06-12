// tests/interpreter/TestPyInterpreter.cpp — PyInterpreter 冒烟/单元测试（含 sys.path 去重验证）

#include "interpreter/PyInterpreter.h"
#include "cplus_alg/python/python_backend.h"

#include <pybind11/embed.h>

#include <gtest/gtest.h>

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
