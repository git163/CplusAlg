#!/usr/bin/env bash
# 一键运行 C++ 和 Python 测试
# 用法: ./scripts/run_tests.sh [build目录]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${1:-${PROJECT_DIR}/build}"

if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "错误: 构建目录不存在: ${BUILD_DIR}" >&2
    echo "请先运行: cmake -B ${BUILD_DIR} && cmake --build ${BUILD_DIR}" >&2
    exit 1
fi

# 优先使用 CMake 找到的 Python 解释器，其次使用环境变量 PYTHON，最后回退到 python3
if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    CMAKE_PYTHON=$(grep -E '^(_?)Python_EXECUTABLE:' "${BUILD_DIR}/CMakeCache.txt" | tail -n1 | sed 's/.*=//' || true)
    if [[ -n "${CMAKE_PYTHON}" && -x "${CMAKE_PYTHON}" ]]; then
        PYTHON_CMD="${CMAKE_PYTHON}"
    else
        PYTHON_CMD="${PYTHON:-python3}"
    fi
else
    PYTHON_CMD="${PYTHON:-python3}"
fi

echo "使用 Python: ${PYTHON_CMD}"

echo "=== C++ 测试 (CTest) ==="
cd "${BUILD_DIR}"
ctest --output-on-failure

echo ""
echo "=== Python 测试 (pytest) ==="
cd "${PROJECT_DIR}"
PYTHONPATH="${PROJECT_DIR}" ${PYTHON_CMD} -m pytest tests/alg -v

echo ""
echo "=== 全部通过 ==="
