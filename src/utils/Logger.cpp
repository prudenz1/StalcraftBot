#include "Logger.h"

std::shared_ptr<spdlog::logger> Logger::s_logger;

void Logger::init(const std::string& logFile) {
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::info);

    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFile, 5 * 1024 * 1024, 3);
    fileSink->set_level(spdlog::level::debug);

    s_logger = std::make_shared<spdlog::logger>(
        "StalcraftBot",
        spdlog::sinks_init_list{consoleSink, fileSink});
    s_logger->set_level(spdlog::level::debug);
    s_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");
    spdlog::set_default_logger(s_logger);

    LOG_INFO("Logger initialized");
}

std::shared_ptr<spdlog::logger>& Logger::get() {
    return s_logger;
}
