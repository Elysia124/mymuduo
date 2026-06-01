cmake_minimum_required(VERSION 3.10.0)

project(mymuduo
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

option(MYMUDUO_ENABLE_MYSQL "Build MySQL utilities" OFF)
option(ENABLE_LOG "Enable mymuduo normal logs at compile time" ON)
option(DEFAULT_ASYNC_LOG "Use async logger by default" OFF)

file(GLOB_RECURSE MYMUDUO_SOURCES
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/src/*.cpp
)

if(NOT MYMUDUO_ENABLE_MYSQL)
    list(FILTER MYMUDUO_SOURCES EXCLUDE REGEX "/utils/db/")
endif()



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

target_compile_definitions(mymuduo
    PUBLIC
        LOG_ENABLED=$<BOOL:${ENABLE_LOG}>
        DEFAULT_ASYNC_LOG=$<BOOL:${DEFAULT_ASYNC_LOG}>
)

if(MYMUDUO_ENABLE_MYSQL)
    find_path(MYSQLCPPCONN_INCLUDE_DIR cppconn/connection.h)
    find_library(MYSQLCPPCONN_LIBRARY mysqlcppconn)
    
    if(NOT MYSQLCPPCONN_INCLUDE_DIR OR NOT MYSQLCPPCONN_LIBRARY)
        message(FATAL_ERROR "MySQL Connector/C++ not found")
    endif()

    target_compile_definitions(mymuduo PUBLIC MYMUDUO_ENABLE_MYSQL=1)
    target_include_directories(mymuduo PUBLIC ${MYSQLCPPCONN_INCLUDE_DIR})
    target_link_libraries(mymuduo PUBLIC ${MYSQLCPPCONN_LIBRARY})
endif()


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

option(MYMUDUO_BUILD_EXAMPLES "Build mymuduo examples" ON)

if(MYMUDUO_BUILD_EXAMPLES)
    add_subdirectory(example/server EXCLUDE_FROM_ALL)
    add_subdirectory(example/client EXCLUDE_FROM_ALL)
    add_subdirectory(example/http EXCLUDE_FROM_ALL)
endif()

option(MYMUDUO_BUILD_TESTS "Build mymuduo tests" OFF)

if(MYMUDUO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)

install(TARGETS mymuduo
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 安装核心头文件，但排除 MySQL 可选模块
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mymuduo
    FILES_MATCHING 
        PATTERN "*.h"
        REGEX "httpserver/utils/db(/|$)" EXCLUDE
        REGEX "httpserver/utils/MysqlUtil\\.h$" EXCLUDE
)

if(MYMUDUO_ENABLE_MYSQL)
    install(FILES
        ${PROJECT_SOURCE_DIR}/include/httpserver/utils/MysqlUtil.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mymuduo/httpserver/utils
    )

    install(DIRECTORY
        ${PROJECT_SOURCE_DIR}/include/httpserver/utils/db/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mymuduo/httpserver/utils/db
        FILES_MATCHING PATTERN "*.h"
    )
endif()


