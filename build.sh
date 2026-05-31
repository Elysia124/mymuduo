#!/bin/bash
set -e
# 开启忽略大小写匹配
shopt -s nocasematch

SOURCE_DIR=$(cd "$(dirname "$0")" && pwd)

# 默认配置
BUILD_TYPE=release
DO_INSTALL=0
CLEAN=0
TARGET=""
MYSQL_ENABLE=false

BUILD_ROOT=${BUILD_ROOT:-"${SOURCE_DIR}/build"}
INSTALL_PREFIX=${INSTALL_PREFIX:-"${SOURCE_DIR}/install"}
CXX=${CXX:-g++}

for arg in "$@"; do
    case "$arg" in
        release)
            BUILD_TYPE=release
            ;;
        debug)
            BUILD_TYPE=debug
            ;;
        install)
            DO_INSTALL=1
            ;;
        clean)
            CLEAN=1
            ;;
        --target=*)
            TARGET="${arg#--target=}"
            ;;
        MySQL_enable)
            MYSQL_ENABLE=true
            ;;
        *)
            echo "Unknown argument: $arg"
            echo "Usage:"
            echo "  ./build.sh"
            echo "  ./build.sh debug"
            echo "  ./build.sh install"
            echo "  ./build.sh clean"
            echo "  ./build.sh --target=testserver"
            echo "  ./build.sh --target=testclient"
            echo "  ./build.sh --target=testhttpserver"
            echo "  ./build.sh mysql_enable"
            exit 1
            ;;
    esac
done

# CMake 的构建类型需要首字母大写
if [ "$BUILD_TYPE" = "debug" ]; then
    CMAKE_BUILD_TYPE=Debug
else
    CMAKE_BUILD_TYPE=Release
fi

# 默认目录：
# Release        -> build/release
# Debug          -> build/debug
BUILD_DIR="${BUILD_ROOT}/${BUILD_TYPE}"


if [ "$CLEAN" -eq 1 ]; then
    echo "Cleaning build directory: ${BUILD_ROOT}"
    rm -rf "${BUILD_ROOT}"
    rm -f "${SOURCE_DIR}/compile_commands.json"
    exit 0
fi

GENERATOR_ARGS=()
if command -v ninja >/dev/null 2>&1; then
    GENERATOR_ARGS=(-G Ninja)
    echo "Generator: Ninja"
else
    GENERATOR_ARGS=(-G "Unix Makefiles")
    echo "Generator: Unix Makefiles"
fi

echo "Source dir     : ${SOURCE_DIR}"
echo "Build dir      : ${BUILD_DIR}"
echo "Build type     : ${CMAKE_BUILD_TYPE}"
echo "Install prefix : ${INSTALL_PREFIX}"
echo "CXX            : ${CXX}"
echo "MySQL-Enable   : ${MYSQL_ENABLE}"

mkdir -p "${BUILD_DIR}"

cmake \
    -S "${SOURCE_DIR}" \
    -B "${BUILD_DIR}" \
    "${GENERATOR_ARGS[@]}" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    -DMYMUDUO_ENABLE_MYSQL="${MYSQL_ENABLE}"

if [ -n "$TARGET" ]; then
    cmake --build "${BUILD_DIR}" --target "${TARGET}" --parallel "$(nproc)"
else
    cmake --build "${BUILD_DIR}" --parallel "$(nproc)"
fi

ln -sf "${BUILD_DIR}/compile_commands.json" "${SOURCE_DIR}/compile_commands.json"

if [ "$DO_INSTALL" -eq 1 ]; then
    cmake --install "${BUILD_DIR}"
fi

echo "Done."