// tests/python/TestTypeConverter.cpp — type_converter 边界测试

#include "cplus_alg/python/type_converter.h"
#include "interpreter/PyInterpreter.h"

#include <pybind11/embed.h>

#include <gtest/gtest.h>

#include <vector>
#include <string>

namespace py = pybind11;

namespace {
void ensure_interpreter_initialized() {
    PyInterpreter& interp = PyInterpreter::Instance();
    if (!interp.IsInitialized()) {
        ASSERT_TRUE(interp.Initialize());
    }
}
} // namespace

class TypeConverterTest : public ::testing::Test {
protected:
    void SetUp() override {
        ensure_interpreter_initialized();
    }
};

TEST_F(TypeConverterTest, NullJsonToPyNone) {
    py::gil_scoped_acquire gil;
    nlohmann::json j = nullptr;
    auto result = cplus_alg::python::py_to_json(
        cplus_alg::python::json_to_py(j));
    EXPECT_TRUE(result.is_null());
}

TEST_F(TypeConverterTest, EmptyBufferMapsToNone) {
    py::gil_scoped_acquire gil;
    cplus_alg::data_buffer buf;
    buf.data = nullptr;
    buf.size_bytes = 0;
    buf.shape = {};
    buf.dtype = "uint8";
    py::object result = cplus_alg::python::input_to_py(buf);
    // input_to_py 对于 data_buffer 返回 dict，其中 "array" 应为 None
    EXPECT_FALSE(result.is_none());
    EXPECT_TRUE(result["array"].is_none());
}

TEST_F(TypeConverterTest, ValidBufferConvertsToArray) {
    py::gil_scoped_acquire gil;
    std::vector<uint8_t> data(100, 42);
    cplus_alg::data_buffer buf;
    buf.data = data.data();
    buf.size_bytes = data.size();
    buf.shape = {10, 10};
    buf.dtype = "uint8";
    py::object result = cplus_alg::python::input_to_py(buf);
    EXPECT_FALSE(result.is_none());
}

TEST_F(TypeConverterTest, AllDtypeBranches) {
    py::gil_scoped_acquire gil;
    constexpr int kN_elements = 100;
    std::vector<uint8_t> data(kN_elements * 8, 0);  // max 8 bytes/elem
    cplus_alg::data_buffer buf;
    buf.data = data.data();
    buf.shape = {10, 10};  // 100 elements

    const std::vector<std::pair<std::string, size_t>> dtypes = {
        {"uint8", 1}, {"int8", 1}, {"uint16", 2}, {"int16", 2},
        {"int32", 4}, {"float32", 4}, {"float64", 8}
    };
    for (const auto& [dt, elem_size] : dtypes) {
        buf.dtype = dt;
        buf.size_bytes = kN_elements * elem_size;
        py::object result = cplus_alg::python::input_to_py(buf);
        EXPECT_FALSE(result.is_none()) << "dtype=" << dt;
    }
}

TEST_F(TypeConverterTest, ShmHandleConvertsToDict) {
    py::gil_scoped_acquire gil;
    cplus_alg::shm_handle handle;
    handle.name = "test_shm";
    handle.shape = {100, 200};
    handle.dtype = "float32";
    handle.size_bytes = 80000;
    py::object result = cplus_alg::python::input_to_py(handle);
    EXPECT_FALSE(result.is_none());
    EXPECT_EQ(py::cast<std::string>(result["type"]), "shm");
}

TEST_F(TypeConverterTest, DeeplyNestedJson) {
    py::gil_scoped_acquire gil;
    // 深度嵌套 JSON 验证不栈溢出
    nlohmann::json deep = nlohmann::json::object();
    nlohmann::json* current = &deep;
    for (int i = 0; i < 100; ++i) {
        (*current)["child"] = nlohmann::json::object();
        current = &(*current)["child"];
    }
    (*current)["value"] = 42;
    py::object result = cplus_alg::python::json_to_py(deep);
    EXPECT_FALSE(result.is_none());
}

TEST_F(TypeConverterTest, JsonRoundTrip) {
    py::gil_scoped_acquire gil;
    nlohmann::json original;
    original["int_val"] = 42;
    original["str_val"] = "hello";
    original["bool_val"] = true;
    original["float_val"] = 3.14;
    original["null_val"] = nullptr;
    original["arr"] = {1, 2, 3};

    py::object py_obj = cplus_alg::python::json_to_py(original);
    nlohmann::json roundtripped = cplus_alg::python::py_to_json(py_obj);

    EXPECT_EQ(roundtripped["int_val"], 42);
    EXPECT_EQ(roundtripped["str_val"], "hello");
    EXPECT_EQ(roundtripped["bool_val"], true);
    EXPECT_DOUBLE_EQ(roundtripped["float_val"].get<double>(), 3.14);
    EXPECT_TRUE(roundtripped["null_val"].is_null());
    EXPECT_EQ(roundtripped["arr"].size(), 3);
}
