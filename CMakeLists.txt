cmake_minimum_required(VERSION 3.10.0)
project(mymuduo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

aux_source_directory(src SRC_LIST)

add_library(mymuduo SHARED ${SRC_LIST})

target_include_directories(mymuduo PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(mymuduo PUBLIC Threads::Threads)
target_compile_options(mymuduo PRIVATE -Wall -Wextra -Wpedantic -g)