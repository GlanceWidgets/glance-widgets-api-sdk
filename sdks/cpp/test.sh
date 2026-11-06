#!/bin/sh
# Build and run every C++ SDK test target. Set SANITIZE=1 for an ASan/UBSan build.
set -eu
sdk_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$sdk_dir/../.." && pwd)
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/glance-cpp-test.XXXXXX")
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
compiler=${CXX:-c++}
flags="-std=c++17 -Wall -Wextra -Wconversion -Wshadow -ffp-contract=off -I $sdk_dir/include"
if [ "${SANITIZE:-0}" = "1" ]; then
  flags="$flags -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=undefined"
else
  flags="$flags -O0"
fi
build() {
  # shellcheck disable=SC2086
  "$compiler" $flags "$sdk_dir/tests/$1.cpp" -o "$build_dir/$1"
}
build test_golden
"$build_dir/test_golden" "$repo_dir"/spec/golden/*.json
for name in test_sdk test_color test_signature test_geometry; do
  build "$name"
  "$build_dir/$name"
done
