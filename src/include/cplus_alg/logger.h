#ifndef CPLUS_ALG_LOGGER_H
#define CPLUS_ALG_LOGGER_H

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace cplus_alg {

// 返回库内默认 logger，首次调用时创建。
// 日志输出到 stderr，级别为 debug。
inline std::shared_ptr<spdlog::logger> default_logger() {
    static auto logger = [] {
        auto log = spdlog::stderr_color_mt("cplus_alg");
        log->set_level(spdlog::level::debug);
        return log;
    }();
    return logger;
}

} // namespace cplus_alg

#endif // CPLUS_ALG_LOGGER_H
