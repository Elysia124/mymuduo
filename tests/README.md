# mymuduo tests

这些测试不依赖 gtest，直接使用 assert 风格的 CHECK 宏。

## 集成方式

在根目录 CMakeLists.txt 末尾加入：

```cmake
option(MYMUDUO_BUILD_TESTS "Build mymuduo tests" OFF)

if(MYMUDUO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

然后执行：

```bash
cmake -S . -B build/tests -G Ninja \
  -DMYMUDUO_BUILD_EXAMPLES=OFF \
  -DMYMUDUO_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build/tests -j$(nproc)
ctest --test-dir build/tests --output-on-failure
```

## 当前测试文件

- Buffer_test.cpp
- Timestamp_test.cpp
- TimerQueue_test.cpp
- EventLoop_test.cpp
- EventLoopThread_test.cpp
- EventLoopThreadPool_test.cpp
- TcpServer_echo_test.cpp
- MultiClient_echo_test.cpp
- LargeMessage_test.cpp
- TcpClient_retry_test.cpp
