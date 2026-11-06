#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
php tests/golden.php
php tests/api.php
# Copy only package sources and use them outside the repository.
task_package_dir=$(mktemp -d)
trap 'rm -rf "$task_package_dir"' EXIT
cp -R src "$task_package_dir/src"
(
  cd "$task_package_dir"
  php -r '
    require "src/GlanceWidgets.php";
    if (count(GlanceWidgets\iconNames()) !== 5287) exit(1);
    if (GlanceWidgets\measureText("12")["height"] !== 5) exit(1);
  '
)
