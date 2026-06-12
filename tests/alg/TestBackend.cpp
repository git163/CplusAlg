#include <cplus_alg/alg_interface.h>
#include <cplus_alg/backend/cpp_backend.h>

#include <optional>
#include <string>

#include <gtest/gtest.h>

TEST(CppBackend, DirectDispatch) {
    cplus_alg::backend::cpp_backend backend;
    cplus_alg::call_params params;
    params.set("value", 42);

    auto result = backend.dispatch(
        "cpp_echo",
        std::nullopt,
        params.json(),
        params.buffers());

    ASSERT_TRUE(result.success);
    EXPECT_FALSE(result.data["has_input"].get<bool>());
    EXPECT_EQ(result.data["value"].get<int>(), 42);
    EXPECT_EQ(result.data["module"].get<std::string>(), "cpp_echo");
}

TEST(CppBackend, UnknownModule) {
    cplus_alg::backend::cpp_backend backend;

    auto result = backend.dispatch(
        "unknown_module",
        std::nullopt,
        nlohmann::json::object(),
        {});

    ASSERT_FALSE(result.success);
    EXPECT_EQ(result.error_type, "ModuleNotFoundError");
}

TEST(AutoDiscovery, CallNewModule) {
    cplus_alg::call_params params;
    params.set("value", std::string("auto_discovered"));

    auto result = cplus_alg::call("test_auto_discovery", params);

    ASSERT_TRUE(result["success"]) << result.value("error", "unknown error");
    EXPECT_EQ(std::string(result["data"]["registered_name"]), "test_auto_discovery");
    EXPECT_EQ(std::string(result["data"]["value"]), "auto_discovered");
}
