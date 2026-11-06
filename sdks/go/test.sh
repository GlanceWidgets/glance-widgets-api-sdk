#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")"
go test -count=1 ./...
out_dir=$(mktemp -d "${TMPDIR:-/tmp}/glance-go.XXXXXX")
trap 'rm -rf "$out_dir"' EXIT
go test -c -o "$out_dir/sdk-test"
(cd /tmp && "$out_dir/sdk-test" -test.run '^TestBundledData$')
