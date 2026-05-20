add_executable(testserver testserver.cpp)

target_link_libraries(testserver
    PRIVATE
        mymuduo
)