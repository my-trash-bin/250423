#!/bin/sh

set -e
cd "$(dirname "$0")"


cmake -B builddir -DCMAKE_INSTALL_PREFIX=builddir/out test
cmake --build builddir --config Debug
cmake --install builddir --config Debug

builddir/out/bin/jsonc test/data/00-basic.json | diff test/data/00-basic.json -
