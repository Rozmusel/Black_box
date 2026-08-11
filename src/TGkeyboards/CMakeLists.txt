add_library(TGkeyboards STATIC TGkeyboards.cpp)

target_include_directories(TGkeyboards PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(TgBot CONFIG REQUIRED)
target_link_libraries(TGkeyboards TgBot::TgBot)

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(TGkeyboards spdlog::spdlog)