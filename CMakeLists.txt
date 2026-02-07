cmake_minimum_required(VERSION 3.10)
project(Black_box LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(${CMAKE_SOURCE_DIR}/lib/bot)
include_directories(${CMAKE_SOURCE_DIR}/lib/archiver)
include_directories(${CMAKE_SOURCE_DIR}/lib/pdf)

# Collect sources (C and C++). Adjust patterns if you add sources elsewhere.
file(GLOB_RECURSE C_SOURCES_RAW
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/lib/bot/*.c
    ${CMAKE_SOURCE_DIR}/lib/bot/**/*.c
)

file(GLOB_RECURSE CPP_SOURCES_RAW
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/lib/**/*.cpp
)

# Exclude any sources coming from nested 'build' directories (generated files)
set(C_SOURCES)
foreach(f ${C_SOURCES_RAW})
    string(FIND "${f}" "/build/" pos)
    string(FIND "${f}" "/lib/bot/curl/" pos2)
    string(FIND "${f}" "/docs/" pos3)
    string(FIND "${f}" "/examples/" pos4)
    if(pos EQUAL -1 AND pos2 EQUAL -1 AND pos3 EQUAL -1 AND pos4 EQUAL -1)
        list(APPEND C_SOURCES ${f})
    endif()
endforeach()

set(CPP_SOURCES)
foreach(f ${CPP_SOURCES_RAW})
    string(FIND "${f}" "/build/" pos)
    string(FIND "${f}" "/lib/bot/curl/" pos2)
    string(FIND "${f}" "/docs/" pos3)
    string(FIND "${f}" "/examples/" pos4)
    string(FIND "${f}" "/lib/pdf/podofo/" pos5)
    if(pos EQUAL -1 AND pos2 EQUAL -1 AND pos3 EQUAL -1 AND pos4 EQUAL -1 AND pos5 EQUAL -1)
        list(APPEND CPP_SOURCES ${f})
    endif()
endforeach()

# Exclude the standalone test driver from the main executable's sources
list(REMOVE_ITEM CPP_SOURCES "${CMAKE_SOURCE_DIR}/src/test_pdf.cpp")

add_executable(main ${C_SOURCES} ${CPP_SOURCES})

# Find libcurl
find_package(CURL REQUIRED)
if (CURL_FOUND)
    target_include_directories(main PRIVATE ${CURL_INCLUDE_DIRS})
    target_link_libraries(main PRIVATE ${CURL_LIBRARIES})
endif()

# Try to find json-c via pkg-config
find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
    pkg_check_modules(JSONC json-c)
    if (JSONC_FOUND)
        target_include_directories(main PRIVATE ${JSONC_INCLUDE_DIRS})
        target_link_libraries(main PRIVATE ${JSONC_LIBRARIES})
    else()
        message(STATUS "pkg-config couldn't find json-c; install libjson-c-dev or adjust links manually")
    endif()
else()
    message(STATUS "pkg-config not found; json-c linking may need manual setup")
endif()

# Find libzip via pkg-config
find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
    pkg_check_modules(LIBZIP libzip)
    if (LIBZIP_FOUND)
        target_include_directories(main PRIVATE ${LIBZIP_INCLUDE_DIRS})
        target_link_libraries(main PRIVATE ${LIBZIP_LIBRARIES})
    else()
        message(FATAL_ERROR "libzip not found; install libzip-dev")
    endif()
else()
    # Fallback: try direct find_package
    find_package(libzip REQUIRED)
    if (libzip_FOUND)
        target_include_directories(main PRIVATE ${LIBZIP_INCLUDE_DIRS})
        target_link_libraries(main PRIVATE ${LIBZIP_LIBRARIES})
    else()
        message(FATAL_ERROR "libzip not found; install libzip-dev")
    endif()
endif()

find_package(podofo REQUIRED)
target_link_libraries(main PRIVATE podofo::podofo)

# Additional linker flags or libraries can be added here, e.g.:
# target_link_libraries(main PRIVATE pthread)
