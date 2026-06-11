#!/usr/bin/env bash
# build.sh — 一键编译、测试并运行 CplusAlg

set -euo pipefail

BUILD_DIR="build"

# 配置 + 编译
cmake -S . -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j

# 运行单元测试
ctest --test-dir "$BUILD_DIR" --output-on-failure

# 运行主程序
./"$BUILD_DIR"/CplusAlg
