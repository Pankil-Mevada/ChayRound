#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# Reuse locally available dependencies without changing the sibling project.
# Override both values for a standalone checkout or a different installation.
deps="${CHAY_DEPS_PREFIX:-$PWD/../FoodService/build-wsl/vcpkg_installed/x64-linux}"
crow="${CROW_INCLUDE_DIR:-$PWD/../FoodService/third_party/Crow/include}"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$deps" -DCROW_INCLUDE_DIR="$crow"
cmake --build build -j "${BUILD_JOBS:-1}"
ctest --test-dir build --output-on-failure
python3 tests/e2e.py
