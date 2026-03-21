#include "Logger.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

std::shared_ptr<spdlog::logger> Logger::s_logger;

void Logger::init(const std::string& logFile) {
#ifdef _WIN32
    // Иначе UTF-8 из логов отображается в консоли как «╨Ш╤Б╤В╨╛╤А╨╕╤П...»
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

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
