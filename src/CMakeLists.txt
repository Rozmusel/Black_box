add_executable(${PROJECT_NAME} main.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

find_package(TgBot CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} TgBot::TgBot)

find_package(SQLiteCpp CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} SQLiteCpp)

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} spdlog::spdlog)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/database $<TARGET_FILE_DIR:${PROJECT_NAME}>/database)

add_subdirectory(database)
target_link_libraries(${PROJECT_NAME} database)

add_subdirectory(TGkeyboards)
target_link_libraries(${PROJECT_NAME} TGkeyboards)