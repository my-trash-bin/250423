#!/bin/sh

set -e
cd "$(dirname "$0")"


cmake -B builddir -DCMAKE_INSTALL_PREFIX=builddir/out test
cmake --build builddir --config Debug
cmake --install builddir --config Debug

ls test/data/ | grep -E '\.json$' | sort | while IFS= read -r line; do
  builddir/out/bin/jsonc test/data/$line | tr -d '\r' | diff test/data/$line.txt -
done
