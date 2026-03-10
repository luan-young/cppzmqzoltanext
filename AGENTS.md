# Repository Guidelines

## Project Structure & Module Organization

This repository is a C++17 library that extends `cppzmq`.

- Public headers: `include/cppzmqzoltanext/*.h` (API surface).
- Implementation: `src/*.cpp` with `src/CMakeLists.txt`.
- Tests: `tests/UTest*.cpp` plus `tests/CMakeLists.txt` (GoogleTest-based).
- Examples: `examples/full_example.cpp` and `examples/CMakeLists.txt`.
- Build/config/docs: top-level `CMakeLists.txt`, `cmake/`, `Doxyfile`, and `docs/`.

Keep new modules paired (`include/.../name.h` + `src/name.cpp`) and align test files as `tests/UTestName.cpp`.

## Build, Test, and Development Commands

Typical local workflow:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCZZE_BUILD_TESTS=ON -DCZZE_BUILD_EXAMPLES=OFF -DCZZE_ENABLE_ASAN=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

Useful options:

- `-DCZZE_BUILD_TESTS=ON`: enables unit tests.
- `-DCZZE_BUILD_EXAMPLES=ON`: builds sample programs.
- `-DCZZE_ENABLE_ASAN=ON`: enables AddressSanitizer for debug runs.

Install artifacts:

```bash
cmake --install build
```

## Coding Style & Naming Conventions

Formatting is defined in `.clang-format` (Google base, 4-space indentation, 120-column limit). Run `clang-format` before opening a PR.

Conventions used in-tree:

- Types use snake_case with `_t` suffix (example: `loop_t`, `actor_t`).
- Header names are lowercase (`poller.h`, `signal.h`).
- Test files use `UTest*.cpp`.

Prefer small, focused translation units and keep public headers minimal and stable.

## Testing Guidelines

Framework: GoogleTest (`gtest`/`gmock`) via CMake FetchContent.

- Place tests under `tests/`.
- Name new files `UTest<Feature>.cpp`.
- Cover success paths and failure/exception paths.

Run all tests with:

```bash
ctest --test-dir build --output-on-failure
```

## Commit & Pull Request Guidelines

- Commit format: `type: concise summary` (for example, `fix: handle actor startup exception propagation`).
- Where relevant, add a longer description in the commit body, explaining the rationale and any non-obvious implementation details.
- Keep commits scoped to one logical change.

For pull requests, include:

- Clear description of behavior changes.
- Linked issue(s) when applicable.
- Test evidence (command + result), and docs updates when API/behavior changes.
