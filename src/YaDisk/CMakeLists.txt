add_library(YaDisk STATIC YaDisk.cpp)

target_include_directories(YaDisk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(yandex-disk-cpp-client CONFIG REQUIRED)
target_link_libraries(YaDisk PRIVATE yandex-disk-cpp-client::yandex-disk-cpp-client)


find_package(spdlog CONFIG REQUIRED)
target_link_libraries(YaDisk PRIVATE spdlog::spdlog)