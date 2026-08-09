add_library(TGkeyboards STATIC TGkeyboards.cpp)

target_include_directories(TGkeyboards PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(TGkeyboards PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(TgBot CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

target_link_libraries(TGkeyboards PRIVATE TgBot::TgBot spdlog::spdlog)