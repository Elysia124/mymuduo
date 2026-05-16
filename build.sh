#!/bin/bash

#!/usr/bin/env bash

set -e

SOURCE_DIR=$(cd "$(dirname "$0")" && pwd)

BUILD_TYPE=Release
DO_INSTALL=0
CLEAN=0

BUILD_ROOT=${BUILD_ROOT:-"${SOURCE_DIR}/build"}
INSTALL_PREFIX=${INSTALL_PREFIX:-"${SOURCE_DIR}/install"}
CXX=${CXX:-g++}

for arg in "$@"; do
    case "$arg" in
        debug|Debug)
            BUILD_TYPE=Debug
            ;;
        release|Release)
            BUILD_TYPE=Release
            ;;
        install)
            DO_INSTALL=1
            ;;
        clean)
            CLEAN=1
            ;;
        *)
            echo "Unknown argument: $arg"
            echo "Usage: ./build.sh [debug|release] [install|clean]"
            exit 1
            ;;
    esac
done

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
echo "Build type     : ${BUILD_TYPE}"
echo "Install prefix : ${INSTALL_PREFIX}"
echo "CXX            : ${CXX}"

mkdir -p "${BUILD_DIR}"

cmake \
    -S "${SOURCE_DIR}" \
    -B "${BUILD_DIR}" \
    "${GENERATOR_ARGS[@]}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"

cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

ln -sf "${BUILD_DIR}/compile_commands.json" "${SOURCE_DIR}/compile_commands.json"

if [ "$DO_INSTALL" -eq 1 ]; then
    cmake --install "${BUILD_DIR}"
fi

echo "Done."