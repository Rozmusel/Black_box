add_library(pdf STATIC pdf.cpp)

target_include_directories(pdf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(podofo CONFIG REQUIRED)
target_link_libraries(pdf PRIVATE podofo::podofo)

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(pdf PRIVATE spdlog::spdlog)