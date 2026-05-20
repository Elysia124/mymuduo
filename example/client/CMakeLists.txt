add_executable(testclient testclient.cpp)

target_link_libraries(testclient
    PRIVATE
        mymuduo
)