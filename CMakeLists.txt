cmake_minimum_required(VERSION 3.10.0)

project(mymuduo
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

file(GLOB MYMUDUO_SOURCES
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/src/*.cpp
)

add_library(mymuduo SHARED ${MYMUDUO_SOURCES})

target_include_directories(mymuduo
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/mymuduo>
)

target_link_libraries(mymuduo
    PUBLIC
        Threads::Threads
)

target_compile_options(mymuduo
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_link_options(mymuduo
    PRIVATE
        -Wl,--no-undefined
)

set_target_properties(mymuduo PROPERTIES
    OUTPUT_NAME mymuduo
)

install(TARGETS mymuduo
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
    DESTINATION include/mymuduo
    FILES_MATCHING PATTERN "*.h"
)