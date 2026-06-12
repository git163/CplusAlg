#ifndef CPLUS_ALG_BACKEND_DISPATCH_RESULT_H_
#define CPLUS_ALG_BACKEND_DISPATCH_RESULT_H_

#include <nlohmann/json.hpp>

#include <string>

namespace cplus_alg {
namespace backend {

// 后端分发的统一返回结构，可在 facade 层转换为 JSON
struct dispatch_result {
    bool success = false;
    nlohmann::json data;
    std::string error;
    std::string error_type;

    static dispatch_result ok(nlohmann::json data) {
        dispatch_result r;
        r.success = true;
        r.data = std::move(data);
        return r;
    }

    static dispatch_result fail(std::string error, std::string error_type = "CxxException") {
        dispatch_result r;
        r.success = false;
        r.error = std::move(error);
        r.error_type = std::move(error_type);
        return r;
    }

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["success"] = success;
        if (success) {
            j["data"] = data;
        } else {
            j["error"] = error;
            j["error_type"] = error_type;
        }
        return j;
    }
};

} // namespace backend
} // namespace cplus_alg

#endif // CPLUS_ALG_BACKEND_DISPATCH_RESULT_H_
