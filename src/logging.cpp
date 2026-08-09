#include <string>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

using namespace std;

void multisink_logger(const string name, const string dir)
{
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();                                    // Консольный вывод логов
    console_sink->set_level(spdlog::level::info);                                                                   // Вывод сообщений с уровнем info и выше
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/logs.txt", true);                    // Вывод тех же логов в файл
    file_sink->set_level(spdlog::level::debug);                                                                     // Вывод сообщений с уровнем debug и выше
    auto logger = std::make_shared<spdlog::logger>("multi_sink", spdlog::sinks_init_list{console_sink, file_sink}); // Объединение
    logger->set_level(spdlog::level::debug);                                                                        // Вывод сообщений с уровнем debug и выше
    spdlog::set_default_logger(logger);                                                                             // Выбор логера по умолчанию
}