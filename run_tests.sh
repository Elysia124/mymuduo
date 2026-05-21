#!/bin/bash
set -e

echo "========== Debug tests =========="
cmake -S . -B build/tests -G Ninja \
  -DMYMUDUO_BUILD_EXAMPLES=OFF \
  -DMYMUDUO_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build/tests -j$(nproc)
ctest --test-dir build/tests --output-on-failure


echo "========== ASAN/UBSAN tests =========="
cmake -S . -B build/asan -G Ninja \
  -DMYMUDUO_BUILD_EXAMPLES=OFF \
  -DMYMUDUO_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"

cmake --build build/asan -j$(nproc)
ctest --test-dir build/asan --output-on-failure


echo "========== TSAN tests =========="
cmake -S . -B build/tsan -G Ninja \
  -DMYMUDUO_BUILD_EXAMPLES=OFF \
  -DMYMUDUO_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer"

cmake --build build/tsan -j$(nproc)
setarch $(uname -m) -R ctest --test-dir build/tsan --output-on-failure


echo "========== Valgrind tests =========="
for t in \
  Buffer_test \
  Timestamp_test \
  TimerQueue_test \
  EventLoop_test \
  EventLoopThread_test \
  EventLoopThreadPool_test \
  TcpServer_echo_test \
  MultiClient_echo_test \
  LargeMessage_test \
  TcpClient_retry_test \
  TimerQueue_cancel_edge_test \
  TcpConnection_forceClose_test \
  TcpConnection_shutdown_test \
  TcpClient_destroy_test \
  ManyClients_roundtrip_test
do
  echo "========== Valgrind: $t =========="
  valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 ./build/tests/tests$t
done

echo "All tests passed."