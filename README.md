# jsonc

This repository contains a small C implementation of a parser for JSON with
comments (jsonc). The source resides in `src/` with a public header in
`include/`. A minimal test program and sample data live under `test/`.

## Building

The recommended way to build and test the project is via the helper script:

```sh
./test.sh
```

This script configures CMake in `builddir`, compiles the parser and example
program and then runs it against the files in `test/data/`.

To generate a `compile_commands.json` for editor integration you can run
`./init.sh` which creates the file in the project root.

## Directory layout

- `include/` – public header `jsonc.h`
- `src/` – parser implementation
- `test/` – simple example program and test data
- `test.sh` – build and regression test script
- `init.sh` – utility for generating `compile_commands.json`
