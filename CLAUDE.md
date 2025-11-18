# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**attest** is a C11 unit testing framework inspired by GoogleTest, with automatic test registration and optional parallel test execution. The project uses `_Generic` for type dispatching and supports GCC/Clang constructor attributes for auto-registration. Thread support is auto-detected (C11 threads, POSIX pthreads, Win32 threads) with a single-threaded fallback for platforms without thread support (e.g., Human68k).

## Build System

The project uses CMake with an out-of-source build strategy:

### Unix-like Systems (macOS, Linux)

- **Configure build**: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
- **Build all**: `cmake --build build`
- **Run all tests**: `./build/attest_selftest`
- **Run tests via ctest**: `ctest --test-dir build --output-on-failure`

### Windows (MSVC)

- **Configure build**: `cmake -S . -B build`
- **Build all (Debug)**: `cmake --build build --config Debug`
- **Build all (Release)**: `cmake --build build --config Release`
- **Run all tests**: `.\build\Debug\attest_selftest.exe` or `.\build\Release\attest_selftest.exe`
- **Run tests via ctest**: `ctest --test-dir build -C Debug --output-on-failure`

### Common Options

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

7. **Parallel Execution** (`src/attest_parallel.c`)
   - Worker thread pool with mutex-protected test queue (POSIX pthreads)
   - Thread-local test contexts (`ATT_THREAD_LOCAL`) for safe parallel execution
   - Dummy stubs for non-threaded environments (`ATT_THREADS_NONE`)

8. **Timeout** (`src/internal/attest_timeout_posix.c`, `src/internal/attest_timeout_win32.c`)
   - Per-test timeout enforcement with platform-specific implementations
   - POSIX: signal-based (`SIGALRM`); Win32: worker thread + event signaling

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

### Editing Files with Tabs

This project uses **tabs for indentation**. When using the Edit tool:

1. **Always extract exact text first**: `sed -n 'N,Mp' <file>` to get lines N through M
2. **Use the extracted output directly** as `old_string` — do not retype or reformat
3. **Why**: Read tool output may not preserve exact whitespace; regenerating code from memory tends to convert tabs to spaces, causing Edit to fail

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
3. Run the full test suite:
   - Unix: `./build/attest_selftest`
   - Windows: `.\build\Debug\attest_selftest.exe`
4. Verify CLI behavior manually:
   - Unix: `./build/attest_selftest --list`, `./build/attest_selftest --filter=Suite.*`
   - Windows: `.\build\Debug\attest_selftest.exe --list`, `.\build\Debug\attest_selftest.exe --filter=Suite.*`
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
- **Thread support**: Auto-detected via `ATT_THREADS_C11` / `ATT_THREADS_POSIX` / `ATT_THREADS_WIN32` / `ATT_THREADS_NONE`. Test contexts use `ATT_THREAD_LOCAL` for thread safety. Parallel execution currently implemented for POSIX pthreads.

## Known Issues

### Cross-frame setjmp in test runner (Resolved)

Earlier releases attributed sporadic ARM64 SIGILL/SIGFPE crashes to a "GCC 14.2.0 sigsetjmp bug" and shipped an `-O1` workaround. A wider compiler audit (GCC 12–15, Clang 14–22 on Apple Silicon) showed the actual root cause was attest itself: `att_context_protect()` was a function that called `setjmp` and returned, leaving the test runner with a `jmp_buf` whose stack frame was already gone. This is undefined behavior, and modern GCC/Clang (12+, Clang 22) optimize on it aggressively.

**Fix**: `att_context_protect()` is now a macro in `src/internal/attest_context.h` that expands to `att_setjmp(*att__get_abort_env_ptr())`, so `setjmp` is called directly in the test-runner's stack frame. The `-O1` workaround in `CMakeLists.txt` and the `-fno-omit-frame-pointer`/`-fno-optimize-sibling-calls` mitigations have been removed.

The same constraint already applied to subtests (`att_subtest_scope_protect`) — see the comment in `include/attest/attest.h`.
