#ifndef CPLUS_ALG_LOGGER_H
#define CPLUS_ALG_LOGGER_H

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace cplus_alg {

// 返回库内默认 logger，首次调用时创建。
// 日志同时输出到 stderr 和 log/cplus_alg.log 文件，级别为 debug。
// 文件按 5MB 滚动，最多保留 3 个历史文件。
inline std::shared_ptr<spdlog::logger> default_logger() {
    static auto logger = [] {
        auto log_dir = std::filesystem::current_path() / "log";
        std::filesystem::create_directories(log_dir);

        auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
        console_sink->set_level(spdlog::level::debug);

        auto log_path = (log_dir / "cplus_alg.log").string();
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_path, 5 * 1024 * 1024, 3);
        file_sink->set_level(spdlog::level::debug);

        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
        auto log = std::make_shared<spdlog::logger>("cplus_alg", sinks.begin(), sinks.end());
        log->set_level(spdlog::level::debug);
        return log;
    }();
    return logger;
}

} // namespace cplus_alg

#endif // CPLUS_ALG_LOGGER_H
