#!/usr/bin/env bash
# build_stdlib.sh build SiS native stdlib modules for one or both platforms.
#
# Usage:
#   ./build_stdlib.sh                  # native platform only
#   ./build_stdlib.sh --windows        # cross-compile for Windows (requires MinGW)
#   ./build_stdlib.sh --all            # native + Windows
#   ./build_stdlib.sh --install /path  # native + install to /path
#   ./build_stdlib.sh --all --install /path/to/sis

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STDLIB_DIR="${SCRIPT_DIR}"
BUILD_DIR="${SCRIPT_DIR}/build/stdlib"
RUNTIME_DIR="${SCRIPT_DIR}/runtime"   # base; /linux and /windows appended by cmake

# Toolchain for cross-compiling stdlib .dlls.
# Intentionally separate from toolchains/windows.cmake (that one sets
# CMAKE_SYSTEM_NAME and cross-root paths for the main binary; we only
# need compiler + RC here).
MINGW_TOOLCHAIN="${SCRIPT_DIR}/toolchains/windows_stdlib.cmake"

SIS_INCLUDE="${SCRIPT_DIR}/include"

DO_NATIVE=true
DO_WINDOWS=false
INSTALL_PREFIX=""

# Argument parsing
while [[ $# -gt 0 ]]; do
  case "$1" in
    --windows) DO_NATIVE=false; DO_WINDOWS=true ;;
    --all)     DO_NATIVE=true;  DO_WINDOWS=true ;;
    --install) INSTALL_PREFIX="$2"; shift ;;
    *) echo "Unknown argument: $1"; exit 1 ;;
  esac
  shift
done

# Native build
if [[ "${DO_NATIVE}" == true ]]; then
  echo "==> Building stdlib (native)..."
  cmake -S "${STDLIB_DIR}" \
        -B "${BUILD_DIR}/native" \
        -DCMAKE_BUILD_TYPE=Release \
        -DSIS_INCLUDE_DIR="${SIS_INCLUDE}" \
        -DSIS_OUTPUT_BASE="${RUNTIME_DIR}"

  cmake --build "${BUILD_DIR}/native" --parallel

  if [[ -n "${INSTALL_PREFIX}" ]]; then
    cmake --install "${BUILD_DIR}/native" --prefix "${INSTALL_PREFIX}"
  fi
fi

# Windows cross-compile
if [[ "${DO_WINDOWS}" == true ]]; then
  if [[ ! -f "${MINGW_TOOLCHAIN}" ]]; then
    echo "Error: ${MINGW_TOOLCHAIN} not found."
    echo "Create it (see toolchains/windows_stdlib.cmake.example) or skip --windows."
    exit 1
  fi

  echo "==> Building stdlib (Windows / MinGW cross-compile)..."
  cmake -S "${STDLIB_DIR}" \
        -B "${BUILD_DIR}/windows" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="${MINGW_TOOLCHAIN}" \
        -DSIS_INCLUDE_DIR="${SIS_INCLUDE}" \
        -DSIS_OUTPUT_BASE="${RUNTIME_DIR}"

  cmake --build "${BUILD_DIR}/windows" --parallel

  if [[ -n "${INSTALL_PREFIX}" ]]; then
    cmake --install "${BUILD_DIR}/windows" --prefix "${INSTALL_PREFIX}"
  fi
fi

echo "==> Done. Modules are in ${RUNTIME_DIR}/"
