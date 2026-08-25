add_library(database STATIC database.cpp)

target_include_directories(database PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(SQLiteCpp CONFIG REQUIRED)
target_link_libraries(database SQLiteCpp)

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(database spdlog::spdlog)