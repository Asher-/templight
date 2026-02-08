# llvm/templight/ — Complete Architecture

## Purpose
Templight — template instantiation profiler for Clang/LLVM. Instruments the compiler to record template instantiation traces for performance analysis.

## Git: `git@github.com:Asher-/templight.git`

## Architecture
- `include/` — Templight headers
- `lib/` — Templight library implementation
- `test/` — Templight profiler tests
- `.clang-format` — code style configuration
- `CMakeLists.txt` — CMake build system

## No PID/Aspect Integration
Pure external tooling — instruments the Clang compiler, not a StrongAI module.

## Dependencies
External: LLVM, Clang (built as part of LLVM)

## Tests: `test/` directory present
