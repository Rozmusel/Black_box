set(BINARY_DIR "${CMAKE_BINARY_DIR}/bin")

add_executable(${PROJECT_NAME} main.cpp TGPatches.cpp logging.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${BINARY_DIR}"
)


find_package(TgBot CONFIG REQUIRED)
target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_CURL)
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