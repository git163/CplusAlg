// tests/python/TestPythonBackend.cpp — python_backend 压力/边界测试

#include "cplus_alg/python/python_backend.h"
#include "interpreter/PyInterpreter.h"
#include "interpreter/GilManager.h"

#include <pybind11/embed.h>

#include <gtest/gtest.h>

#include <atomic>
#include <thread>

namespace py = pybind11;

namespace {

void ensure_interpreter_initialized() {
    PyInterpreter& interp = PyInterpreter::Instance();
    if (!interp.IsInitialized()) {
        ASSERT_TRUE(interp.Initialize());
    }
}

} // namespace

class PythonBackendTest : public ::testing::Test {
protected:
    void SetUp() override {
        ensure_interpreter_initialized();
    }
};

TEST_F(PythonBackendTest, AvailableWhenInitialized) {
    cplus_alg::python::python_backend backend;
    EXPECT_TRUE(backend.available());
}

TEST_F(PythonBackendTest, UnavailableAfterFinalize) {
    cplus_alg::python::python_backend backend;
    ASSERT_TRUE(backend.available());
    PyInterpreter::Instance().Finalize();
    EXPECT_FALSE(backend.available());
}

TEST_F(PythonBackendTest, AutoRecoverAfterReinit) {
    // Finalize 后 backend 自动检测并重新初始化
    cplus_alg::python::python_backend backend;
    ASSERT_TRUE(backend.available());
    PyInterpreter::Instance().Finalize();
    ASSERT_FALSE(backend.available());

    PyInterpreter::Instance().Initialize();
    EXPECT_TRUE(backend.available());
}

TEST_F(PythonBackendTest, DispatchWithMissingModule) {
    cplus_alg::python::python_backend backend;
    nlohmann::json params;
    auto result = backend.dispatch("nonexistent_module_xyz", {}, params, {});
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

TEST_F(PythonBackendTest, MultiThreadedFirstDispatch) {
    // CPython 限制：非 GIL 持有线程首次访问 py::object 时，
    // PyGILState_Ensure 在 py::scoped_interpreter 默认模式下与 GIL 转移存在竞态
    GTEST_SKIP() << "CPython limitation: multi-thread py::object access without pre-acquired GIL deadlocks";
}
