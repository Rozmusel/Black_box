#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"

using namespace std;

void multisink_logger(const string name, const string dir)
{
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();                                    // Консольный вывод логов
    console_sink->set_level(spdlog::level::info);                                                                   // Вывод сообщений с уровнем info и выше
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/logs.txt", false);                   // Общий лог
    file_sink->set_level(spdlog::level::info);                                                                      // Вывод сообщений с уровнем info и выше
    auto debug_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "logs/debug/debug.log", 10 * 1024 * 1024, 3, false);                                                            // Debug-лог до 40 МБ
    debug_sink->set_level(spdlog::level::debug);                                                                    // Вывод сообщений с уровнем debug и выше
    auto logger = std::make_shared<spdlog::logger>(
        "multi_sink", spdlog::sinks_init_list{console_sink, file_sink, debug_sink});                              // Объединение
    logger->set_level(spdlog::level::debug);                                                                        // Разрешить debug-сообщения для debug-файла
    spdlog::set_default_logger(logger);                                                                             // Выбор логера по умолчанию
}

std::string stringDate(int hours_offset)
{
    std::ostringstream result;
    const auto now = std::chrono::system_clock::now() + std::chrono::hours(hours_offset);
    const std::time_t time = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};

    #ifdef _WIN32
    localtime_s(&localTime, &time);
    #else
    localtime_r(&time, &localTime);
    #endif

    result << std::put_time(&localTime, "%Y-%m-%d");
    spdlog::debug("Generated date string for '{}': {}", hours_offset, result.str());
    return result.str();
}

