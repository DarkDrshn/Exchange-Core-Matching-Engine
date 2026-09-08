#!/usr/bin/env bash

set -euo pipefail

build_type="${1:-debug}"

case "${build_type}" in
    debug|release)
        ;;
    *)
        printf 'Usage: %s [debug|release]\n' "$0" >&2
        exit 2
        ;;
esac

project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_directory="${project_root}/build/${build_type}"

if [[ -f "${build_directory}/CMakeCache.txt" ]] &&
   ! grep -q '^CMAKE_GENERATOR:INTERNAL=Unix Makefiles$' "${build_directory}/CMakeCache.txt"; then
    rm -rf -- "${build_directory}"
fi

cmake --preset "${build_type}" "${project_root}"
cmake --build "${build_directory}" --parallel
ctest --test-dir "${build_directory}" --output-on-failure