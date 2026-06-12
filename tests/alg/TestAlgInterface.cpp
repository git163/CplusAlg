#include <cplus_alg/alg_interface.h>
#include <cplus_alg/data_buffer.h>
#include <cplus_alg/shm_handle.h>

#include <cstdint>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

TEST(AlgInterface, CallParamsJson) {
    cplus_alg::call_params params;
    params.set("method", std::string("ccorr_normed"))
          .set("threshold", 0.5);

    EXPECT_EQ(params.json()["method"], "ccorr_normed");
    EXPECT_DOUBLE_EQ(params.json()["threshold"], 0.5);
}

TEST(AlgInterface, CallParamsBuffer) {
    std::vector<uint8_t> data(100);
    cplus_alg::data_buffer buf{
        .shape = {10, 10},
        .dtype = "uint8",
        .data = data.data(),
        .size_bytes = data.size()};

    cplus_alg::call_params params;
    params.set_buffer("template", buf);

    EXPECT_EQ(params.buffers().size(), 1u);
    EXPECT_EQ(params.buffers().at("template").size_bytes, 100u);
}

TEST(AlgInterface, DataBufferAccess) {
    int value = 42;
    cplus_alg::data_buffer buf{
        .shape = {1},
        .dtype = "int32",
        .data = &value,
        .size_bytes = sizeof(int)};

    EXPECT_EQ(buf.shape[0], 1);
    EXPECT_EQ(buf.dtype, "int32");
    EXPECT_EQ(buf.size_bytes, sizeof(int));
}

TEST(ShmBuffer, CreateAndDestroy) {
    std::string name = "cplusalg_test_create";
    {
        cplus_alg::shm_buffer buffer(name, 1024);
        EXPECT_NE(buffer.data(), nullptr);
        EXPECT_EQ(buffer.size(), 1024u);
        EXPECT_EQ(buffer.name(), name);
    }
    // 析构后 shm 应已被删除，重新创建同名不应失败
    {
        cplus_alg::shm_buffer buffer(name, 512);
        EXPECT_EQ(buffer.size(), 512u);
    }
}

TEST(AlgInterface, NoInputCall) {
    cplus_alg::call_params params;
    params.set("value", 42);

    auto result = cplus_alg::call("echo", params);

    ASSERT_TRUE(result["success"]) << result.value("error", "unknown error");
    EXPECT_FALSE(result["data"]["has_input"]);
    EXPECT_EQ(result["data"]["input_type"], nullptr);
    EXPECT_EQ(result["data"]["value"], 42);
}

TEST(AlgInterface, DirectTransmitTag) {
    std::vector<std::uint8_t> data = {1, 2, 3, 4};
    cplus_alg::data_buffer buf{
        .shape = {2, 2},
        .dtype = "uint8",
        .data = data.data(),
        .size_bytes = data.size()};

    cplus_alg::call_params params;
    params.set("value", std::string("direct"));

    auto result = cplus_alg::call(cplus_alg::direct_transmit, "echo", buf, params);

    ASSERT_TRUE(result["success"]) << result.value("error", "unknown error");
    EXPECT_TRUE(result["data"]["has_input"]);
    EXPECT_EQ(std::string(result["data"]["input_type"]), "buffer");
    EXPECT_EQ(result["data"]["shape"][0], 2);
    EXPECT_EQ(result["data"]["shape"][1], 2);
    EXPECT_EQ(result["data"]["first_element"], 1);
    EXPECT_EQ(std::string(result["data"]["value"]), "direct");
}

TEST(AlgInterface, ShmTransmitTag) {
    // 构造大于 1MB 的缓冲区，强制走共享内存
    constexpr std::size_t k_size = 2 * 1024 * 1024;
    auto data = std::make_unique<std::vector<std::uint8_t>>(k_size, 7);
    cplus_alg::data_buffer buf{
        .shape = {static_cast<int>(k_size)},
        .dtype = "uint8",
        .data = data->data(),
        .size_bytes = data->size()};

    cplus_alg::call_params params;
    params.set("value", std::string("shm"));

    auto result = cplus_alg::call(cplus_alg::shm_transmit, "echo", buf, params);

    ASSERT_TRUE(result["success"]) << result.value("error", "unknown error");
    EXPECT_TRUE(result["data"]["has_input"]);
    EXPECT_EQ(std::string(result["data"]["input_type"]), "shm");
    EXPECT_EQ(std::string(result["data"]["value"]), "shm");
}

TEST(AlgInterface, AutoTransmitChoosesDirectForSmallData) {
    std::vector<std::uint8_t> data = {5, 6, 7, 8};
    cplus_alg::data_buffer buf{
        .shape = {4},
        .dtype = "uint8",
        .data = data.data(),
        .size_bytes = data.size()};

    auto result = cplus_alg::call("echo", buf);

    ASSERT_TRUE(result["success"]) << result.value("error", "unknown error");
    EXPECT_TRUE(result["data"]["has_input"]);
    EXPECT_EQ(std::string(result["data"]["input_type"]), "buffer");
    EXPECT_EQ(result["data"]["first_element"], 5);
}
