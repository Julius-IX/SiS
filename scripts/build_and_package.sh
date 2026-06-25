#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
STDLIB_DIR="$PROJECT_ROOT/stdlib"

DO_LINUX=false
DO_WINDOWS=false

usage() {
    echo "Usage: $0 [--linux] [--windows] [--all]"
    exit 1
}

[[ $# -eq 0 ]] && usage

while [[ $# -gt 0 ]]; do
    case "$1" in
        --linux)   DO_LINUX=true ;;
        --windows) DO_WINDOWS=true ;;
        --all)     DO_LINUX=true; DO_WINDOWS=true ;;
        *) echo "Unknown argument: $1"; usage ;;
    esac
    shift
done

# Build main binary

cd "$PROJECT_ROOT"

if [[ "$DO_LINUX" == true ]]; then
    echo "==> Building Linux binary..."
    cmake --preset release
    cmake --build --preset release
fi

if [[ "$DO_WINDOWS" == true ]]; then
    echo "==> Building Windows binary..."
    cmake --preset windows
    cmake --build --preset windows
fi

# Build stdlib

echo "==> Building stdlib..."

if   [[ "$DO_LINUX" == true  && "$DO_WINDOWS" == true  ]]; then STDLIB_ARGS="--all"
elif [[ "$DO_WINDOWS" == true ]];                               then STDLIB_ARGS="--windows"
else                                                                 STDLIB_ARGS=""
fi

bash "$STDLIB_DIR/build_stdlib.sh" $STDLIB_ARGS

# Package

package_linux() {
    local ARCHIVE="$PROJECT_ROOT/sis-linux-x86_64.tar.gz"
    local STAGING
    STAGING=$(mktemp -d)
    trap 'rm -rf "$STAGING"' RETURN

    echo "==> Packaging Linux → $(basename "$ARCHIVE")..."

    mkdir -p "$STAGING/dynamic" "$STAGING/managed"

    cp "$PROJECT_ROOT/build/Release/SiS"    "$STAGING/sis"
    cp -r "$STDLIB_DIR/runtime/linux/."     "$STAGING/dynamic/"
    cp -r "$STDLIB_DIR/managed/."           "$STAGING/managed/"

    tar -czf "$ARCHIVE" -C "$STAGING" .

    echo "    Created: $ARCHIVE"
}

package_windows() {
    local ARCHIVE="$PROJECT_ROOT/sis-windows-x86_x64.zip"
    local STAGING
    STAGING=$(mktemp -d)
    trap 'rm -rf "$STAGING"' RETURN

    echo "==> Packaging Windows → $(basename "$ARCHIVE")..."

    mkdir -p "$STAGING/dynamic" "$STAGING/managed"

    cp "$PROJECT_ROOT/build/windows/SiS.exe"  "$STAGING/sis.exe"
    cp -r "$STDLIB_DIR/runtime/windows/."     "$STAGING/dynamic/"
    cp -r "$STDLIB_DIR/managed/."             "$STAGING/managed/"

    (cd "$STAGING" && zip -r "$ARCHIVE" .)

    echo "    Created: $ARCHIVE"
}

[[ "$DO_LINUX"   == true ]] && package_linux
[[ "$DO_WINDOWS" == true ]] && package_windows

echo ""
echo "Done."
