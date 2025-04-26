#!/bin/sh

set -e

cd "$(dirname "$0")"

PWD_ESCAPED="$(
  (command -v cmd >/dev/null 2>&1 &&  powershell -NoProfile -Command 'cmd /c cd' || pwd) \
    | sed 's#\\#\\\\#g'
)"

echo '[{"directory":"'"$PWD_ESCAPED"'","arguments":["clang","-Iinclude","-x","c","-std=c99","-Wall","-Wextra","-Werror","-pedantic","-c","file.c","-o","file.o"],"file":"file.c" }]' > compile_commands.json
