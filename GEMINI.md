# GEMINI.md

## Project Overview

This project is a lightweight C11 unit testing framework named "attest". It is inspired by GoogleTest and designed for simplicity and automatic test registration. The framework provides a simple API with macros like `TEST()` and `TEST_F()`, a rich set of type-generic assertions, and support for test fixtures. It has zero external dependencies and uses CMake for its build process.

The project is structured as follows:
- `src/`: Contains the source code for the framework.
- `include/`: Contains the public header files.
- `tests/`: Contains the self-tests for the framework.
- `docs/`: Contains design and specification documents.
- `CMakeLists.txt`: The main build script for the project.

## Building and Running

The project uses CMake for building. The following commands can be used to build and run the tests:

```bash
# Configure the project
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build the project
cmake --build build

# Run the self-tests
./build/attest_selftest
```

## Development Conventions

- **Coding Style:** The project follows the WebKit code style (tabs, width 4, right pointer alignment). `clang-format` is used to enforce the style.
- **Testing:** New features should be accompanied by self-tests in `tests/selftest_main.c`. The full test suite can be run with `./build/attest_selftest`.
- **Commit Messages:** The project follows the Conventional Commits specification for commit messages.
