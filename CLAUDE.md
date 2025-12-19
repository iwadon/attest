# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**attest** is a C11 unit testing framework inspired by GoogleTest, designed for single-threaded execution with automatic test registration. The project uses `_Generic` for type dispatching and supports GCC/Clang constructor attributes for auto-registration.

## Build System

The project uses CMake with an out-of-source build strategy:

- **Configure build**: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
- **Build all**: `cmake --build build`
- **Run all tests**: `./build/attest_selftest`
- **Run tests via ctest**: `ctest --test-dir build --output-on-failure`
- **Build testing toggle**: Use `-DATTEST_BUILD_TESTING=ON/OFF` to control test compilation. Default is ON when attest is the top-level project, OFF when used as a subproject.

## Architecture

### Core Components

1. **Test Registry** (`src/internal/attest_registry.c`)
   - Manages test registration and storage
   - Tracks test cases with suite names, test names, file locations
   - Frozen after initialization to prevent late registration

2. **Execution Engine** (`src/attest.c`)
   - Implements `attest_main()` entry point
   - Uses `setjmp/longjmp` for fatal assertion handling
   - `ASSERT_*` macros abort the current test via longjmp
   - `EXPECT_*` macros continue execution after failure
   - Tests run in registration order

3. **CLI Parser** (`src/attest_cli.c`)
   - Handles `--list`, `--filter=<pattern>`, `--no-color`
   - Wildcard filtering: `*` and `?` support with shorthand (`Suite` → `Suite.*`, `.Name` → `*.Name`)
   - Multiple filters separated by `;` (OR logic)

4. **Assertions** (`src/attest_assert.c`)
   - Type-generic comparison via `_Generic` macro
   - Dispatches to specialized handlers: `att_handle_compare_signed`, `att_handle_compare_unsigned`, `att_handle_compare_double`, `att_handle_compare_pointer`
   - String/memory/floating-point specialized assertions

5. **Output Capture** (`src/attest_capture.c`)
   - `att_capture_begin()` and `att_capture_end()` redirect stderr to buffer
   - Used by `ATT_EXPECT_SUBTEST_FAILS` macro for isolated test validation
   - Non-reentrant (no nesting allowed)

6. **Fixtures** (`src/attest_fixture.c`)
   - `ATT_FIXTURE_SETUP(FixtureName)` and `ATT_FIXTURE_TEARDOWN(FixtureName)` define setup/teardown hooks
   - `TEST_F(FixtureName, TestName)` runs test with fixture context
   - Fixture data is allocated on the stack and passed to test body

### Public API Surface

- **Test definition**: `TEST(Suite, Name)` for simple tests, `TEST_F(Fixture, Name)` for fixture-based tests
- **Assertions**: `ASSERT_*` (fatal), `EXPECT_*` (non-fatal) variants for EQ/NE/LT/LE/GT/GE, TRUE/FALSE, STREQ/STRNE, MEMEQ, NEAR
- **Subtest API**: `att_run_subtest()` for isolated test execution, `ATT_EXPECT_SUBTEST_FAILS(name, block, min, max)` for expected-failure validation
- **Skip API**: `ATT_SKIP(reason)` and `ATT_SKIP_IF(cond, reason)` for conditional skipping
- **Scoped info**: `SCOPED_INFO(fmt, ...)` for context that appears only on failure (uses GCC cleanup attribute)
- **Output capture**: `att_capture_begin()` / `att_capture_end()` for stderr redirection
- **Manual registration**: `ATT_REGISTER_TESTS(...)` for platforms without constructor attribute support

## Test Organization

- **Self-tests**: `tests/selftest_main.c` contains comprehensive validation of the framework itself
- **Test naming**: Follow `Suite.Name` pattern (e.g., `Assert.Numbers`, `Fixture.SetupTeardown`)
- **Fixtures**: Define setup/teardown with `ATT_FIXTURE_SETUP(Type)` and `ATT_FIXTURE_TEARDOWN(Type)`, then use `TEST_F(Type, TestName)`

## Code Style

- **Formatting**: WebKit-based style with tabs (width 4), right pointer alignment
- **Before committing**: Run `clang-format -i <file>` on modified C files
- **Naming conventions**:
  - Public API: `att_` prefix with lowercase snake_case (`att_summary`, `att_run_subtest`)
  - Macros: UPPER_SNAKE_CASE (`ASSERT_EQ`, `ATT_EXPECT_SUBTEST_FAILS`)
  - Files: snake_case (`attest_assert.c`, `attest_internal.h`)
- **Line length**: Target 100 columns for code

## Documentation

The project documentation is organized in `docs/`:

- **`docs/api.md`**: Complete API reference (assertions, macros, CLI options)
- **`docs/guide.md`**: User guide with usage patterns and examples
- **`docs/internals.md`**: Architecture, platform support, design decisions
- **`docs/roadmap.md`**: Future plans (P1+)

## Development Workflow

When implementing new features:

1. Add self-tests in `tests/selftest_main.c` that exercise the new functionality
2. For bug fixes, add a regression test that reproduces the issue before fixing
3. Run the full test suite: `./build/attest_selftest`
4. Verify CLI behavior manually: `./build/attest_selftest --list`, `./build/attest_selftest --filter=Suite.*`
5. Format code with `clang-format -i` before committing
6. Tag TODOs with the planned stage: `// TODO(P1+): implement advanced filtering`

## Exit Codes

- **0**: All tests passed or `--list` mode
- **1**: One or more test failures
- **2**: CLI parsing error (unknown option)
- **3**: Initialization failure (duplicate test names, internal errors)

## Key Implementation Details

- **Auto-registration**: Uses GCC/Clang `__attribute__((constructor))` for automatic test discovery. MSVC uses `.CRT$XCU` section. Fallback to manual registration via `ATT_REGISTER_TESTS()` for other compilers.
- **Fatal vs Non-Fatal**: `ASSERT_*` uses `longjmp` to abort the test immediately; `EXPECT_*` records failure and continues
- **Test isolation**: Each `TEST()` runs in its own `setjmp` context. Subtests via `att_run_subtest()` run in nested contexts and don't affect parent test execution.
- **String comparison**: `NULL` == `NULL` is true; `NULL` vs non-NULL is false; output shows `"(null)"` for NULL pointers
- **Floating-point**: `NEAR` assertions use `fabs(a - b) <= epsilon`; NaN always fails; ±infinity matches only with same sign

## Known Issues

### GCC 14.2.0 ARM64 sigsetjmp/siglongjmp Bug

**Platform**: ARM64/aarch64 (Ubuntu 25.04)
**Compiler**: GCC 14.2.0
**Symptom**: SIGILL crash when running full test suite, individual tests work fine
**Cause**: GCC 14.2.0 ARM64 backend has a bug in `sigsetjmp/siglongjmp` implementation that corrupts the return address (x30 register) and stack frame
**Workaround**:
1. **Recommended**: Use Clang instead of GCC on ARM64 platforms
2. **Alternative**: Use GCC 13.x or wait for GCC 14.3+ with fix
3. **Applied mitigations** (partial workaround):
   - `sigjmp_buf` with explicit 16-byte alignment and `volatile` qualifier
   - Structure and static variable alignment attributes
   - `-fno-omit-frame-pointer` and `-fno-optimize-sibling-calls` flags
   - `-O1` optimization level for GCC 14.x on ARM64 (automatic)
   - C11 `aligned_alloc()` for heap-allocated contexts on Linux

**Implementation**: See `src/attest_assert.c:17-42` and `CMakeLists.txt:24-39`
