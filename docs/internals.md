# Internals

This document describes the architecture, design decisions, and platform-specific implementation details of attest.

## Architecture Overview

```
attest/
├── include/attest/
│   └── attest.h           # Public API
├── src/
│   ├── attest.c           # Entry point, test execution loop
│   ├── attest_main.c      # main() wrapper calling attest_main()
│   ├── attest_cli.c       # CLI parsing
│   ├── attest_assert.c    # Assertion implementations, context management
│   ├── attest_capture.c   # stderr capture
│   ├── attest_fixture.c   # Fixture registry and execution
│   ├── attest_parallel.c  # Parallel execution (worker pool)
│   └── internal/
│       ├── attest_internal.h      # Internal types, platform macros
│       ├── attest_context.h       # Context types, timeout/fixture accessors
│       ├── attest_timeout.h       # Platform-agnostic timeout interface
│       ├── attest_timeout_posix.c # POSIX timeout (SIGALRM, includes Human68k stubs)
│       ├── attest_timeout_win32.c # Windows timeout (thread + event)
│       └── attest_registry.c      # Test registration storage
└── tests/
    └── selftest_main.c    # Framework self-tests
```

## Core Components

### Test Registry

**File:** `src/internal/attest_registry.c`

- Stores registered test cases with suite name, test name, file location
- Frozen after initialization (no late registration)
- Tests stored in registration order

### Execution Engine

**File:** `src/attest.c`

- `attest_main()` entry point
- Uses `setjmp/longjmp` for fatal assertion handling
- Each test runs in its own `setjmp` context
- Execution flow: registry freeze → filter → run → summarize → exit code

### Assertion System

**File:** `src/attest_assert.c`

- `_Generic` macro dispatches to type-specific handlers:
  - `att_handle_compare_signed` — signed integers (cast to `long long`)
  - `att_handle_compare_unsigned` — unsigned integers (cast to `unsigned long long`)
  - `att_handle_compare_double` — `float` / `double`
  - `att_handle_compare_long_double` — `long double`
  - `att_handle_compare_pointer` — pointers (compared as `uintptr_t`)
- Context state tracks: current test, failure counts, longjmp buffer, timeout state, info stack
- The signed / unsigned / double / long-double comparator, formatter, and
  handler families share a single textual template each. They are emitted
  by file-local `ATT_DEFINE_COMPARE`, `ATT_DEFINE_FORMATTER`, and
  `ATT_DEFINE_HANDLER` macros (see `src/attest_assert.c`), so adding a new
  numeric type reduces to one macro invocation per family. Pointer, bool,
  and string handlers stay hand-written because they need bespoke
  formatting (hex addresses, `(null)` sentinels, multi-line diff output).

### CLI Parser

**File:** `src/attest_cli.c`

- Parses `--list`, `--filter`, `--no-color`, `--timeout-ms`, `--shuffle`, `--jobs`, `--format`, `--output`
- `--format` accepts `default`, `tap`, or `junit`; `--output` is only valid
  with `--format=junit` and otherwise produces an error
- `--jobs=auto` and `--jobs=0` both resolve to the detected CPU count via
  `att_get_cpu_count()` (sysconf on POSIX, `GetSystemInfo` on Windows)
- Filter syntax: wildcards (`*`, `?`), shorthand (`Suite` → `Suite.*`), multiple patterns (`;` separator), negative filters (`-Pattern`)
- Unknown options → exit code 2

### Fixtures

**File:** `src/attest_fixture.c`

- Fixture entries registered at startup
- Setup/teardown functions looked up by fixture type name
- Teardown always runs (even after ASSERT failure or skip)

### Output Capture

**File:** `src/attest_capture.c`

- `att_capture_begin()` redirects stderr to internal buffer
- `att_capture_end()` returns captured content
- Non-reentrant (nesting not supported)

### Parallel Execution

**File:** `src/attest_parallel.c`

- Worker pool architecture with configurable thread count
- Each worker has thread-local context (`g_ctx`)
- Results collected per-test, output in registration order
- Mutex-protected work queue for test distribution
- Compiled and entered only under `ATT_THREADS_POSIX`. Other thread backends
  (`ATT_THREADS_C11`, `ATT_THREADS_WIN32`, `ATT_THREADS_NONE`) currently fall
  through to the sequential runner regardless of `--jobs=N`.
- The parallel runner emits only the default human-readable format. TAP
  per-test lines and JUnit XML output are produced exclusively by the
  sequential path, so requesting `--format=tap` or `--format=junit` together
  with `--jobs > 1` is not supported.

---

## Platform Support

### Platform Detection

Defined in `src/internal/attest_internal.h`:

| Macro | Meaning |
|-------|---------|
| `ATT_PLATFORM_WINDOWS` | Windows (any) |
| `ATT_PLATFORM_POSIX` | POSIX-compliant (Linux, macOS, etc.) |
| `ATT_PLATFORM_HUMAN68K` | Sharp X680x0 (Human68k) — single-threaded, no timeout |
| `ATT_COMPILER_MSVC` | Microsoft Visual C++ |
| `ATT_COMPILER_GCC_LIKE` | GCC or Clang |

### Thread Support Detection

| Macro | Condition |
|-------|-----------|
| `ATT_THREADS_C11` | C11 `<threads.h>` available |
| `ATT_THREADS_POSIX` | POSIX threads (`pthread`) |
| `ATT_THREADS_WIN32` | Windows threads |
| `ATT_THREADS_NONE` | No thread support |

### setjmp/longjmp Abstraction

POSIX uses `sigsetjmp/siglongjmp` to preserve signal masks; Windows uses standard `setjmp/longjmp`.

```c
typedef att_jmp_buf;
#define att_setjmp(env)      // Platform-specific
#define att_longjmp(env, v)  // Platform-specific
```

**Critical constraint:** `setjmp` must be called directly in the caller's stack frame (not through a function call). This is enforced via macro expansion.

### Compiler Attributes

| Attribute | GCC/Clang | MSVC |
|-----------|-----------|------|
| Constructor | `__attribute__((constructor))` | `.CRT$XCU` section |
| Alignment | `__attribute__((aligned(n)))` | `__declspec(align(n))` |
| Cleanup | `__attribute__((cleanup(fn)))` | Not supported |
| Thread-local | `__thread` or `_Thread_local` | `__declspec(thread)` |

### Memory Allocation

Aligned allocation for context structures:

| Platform | Function |
|----------|----------|
| Linux/GCC | `aligned_alloc()` (C11) |
| MSVC | `_aligned_malloc()` / `_aligned_free()` |
| Other | Standard `malloc()` / `free()` |

---

## Timeout Implementation

### POSIX

- Uses `sigaction()` and `setitimer(ITIMER_REAL)`
- Signal handler sets timeout flag and calls `longjmp`
- Works for infinite loops (signal interrupts execution)

### Windows

- Spawns a dedicated timer thread with `_beginthreadex()`
- The timer thread waits on a manual-reset `CreateEvent()` handle via
  `WaitForSingleObject(event, timeout_ms)`. `WAIT_TIMEOUT` means the deadline
  expired — the thread then sets the `triggered` flag and re-signals the
  event so the main thread can observe it. `WAIT_OBJECT_0` means
  `att_timeout_stop()` cancelled the wait early.
- The main thread polls the same event from `att_context_record_assert()`
  using `WaitForSingleObject(event, 0)`, throttled to once every 32 assertions
  to keep the per-assertion cost negligible. On a hit, it calls
  `att_context_abort()` to longjmp back to the test runner.
- **Limitation:** Because the main thread only learns about the timeout when
  it next executes an assertion macro, a tight loop with no assertions cannot
  be interrupted on Windows. The timer thread itself still fires, but the
  failure is recorded only when control returns through an assertion or to
  `att_context_end()`. For long-running code paths, ensure periodic
  assertions (or `EXPECT_TRUE(true)` checkpoints) so the timeout can take
  effect.

---

## Parallel Execution Design

### Architecture

```
[Main Thread]
     │
     ├─ Freeze registry
     ├─ Initialize work queue (mutex-protected index)
     ├─ Allocate results array
     │
     ├─────────────────────────────────────────────┐
     ▼                                             ▼
[Worker 1]                                    [Worker N]
     │                                             │
     ├─ Loop:                                      ├─ Loop:
     │   ├─ Lock mutex                             │   ├─ Lock mutex
     │   ├─ Get next test index                    │   ├─ Get next test index
     │   ├─ Unlock mutex                           │   ├─ Unlock mutex
     │   ├─ Init thread-local context              │   ├─ Init thread-local context
     │   ├─ Run test                               │   ├─ Run test
     │   └─ Store result                           │   └─ Store result
     │                                             │
     └─────────────────────────────────────────────┘
                          │
                          ▼
                   [Main Thread]
                          │
                          ├─ Join all workers
                          ├─ Output results (registration order)
                          └─ Return summary
```

### Thread-Local Storage

Each worker thread has its own `g_ctx`:

```c
#if defined(ATT_THREADS_C11)
    _Thread_local att_context_state *g_ctx;
#elif defined(ATT_THREADS_POSIX)
    __thread att_context_state *g_ctx;
#elif defined(ATT_THREADS_WIN32)
    __declspec(thread) att_context_state *g_ctx;
#endif
```

### Output Buffering

- Each test's output is captured to a per-test buffer
- After all workers complete, main thread outputs in registration order
- Prevents interleaved output from concurrent tests

---

## Known Issues

### Cross-frame setjmp in the test runner (Resolved)

**Symptom:** SIGILL / SIGFPE / SIGSEGV when running the full self-test suite
on Apple Silicon, observed across GCC 12.5, 13.4, 15.2 and Clang 22.1 at
`-O2` and above. GCC 14.2 (an earlier point release) and a regression in
Clang 22 made this very visible; previous releases attributed the symptom to
"GCC 14.2.0 sigsetjmp bug" and shipped an `-O1` ARM64 workaround in
`CMakeLists.txt`.

**Root cause:** attest's own test runner. The original `att_context_protect()`
was an out-of-line function in `src/attest_assert.c` that called `setjmp`
and returned. By the time the runner in `src/attest.c` (or
`src/attest_parallel.c`) tried to `longjmp` back, the `jmp_buf` referred to
a stack frame that no longer existed. This is undefined behavior, and
modern GCC/Clang optimizers happily reason on the assumption that it never
happens — so they hoist or eliminate code that the runner relied on.

**Fix:** `att_context_protect()` is now a macro in
`src/internal/attest_context.h`:

```c
#define att_context_protect() att_setjmp(*att__get_abort_env_ptr())
```

Both `attest.c` and `attest_parallel.c` invoke this macro inside the same
stack frame that handles the `longjmp`, so the `jmp_buf` stays valid.
The `-O1` workaround, the `-fno-omit-frame-pointer` /
`-fno-optimize-sibling-calls` mitigations, and the explicit 16-byte
`sigjmp_buf` alignment guards have all been removed; verified across
GCC 12–15 and Clang 16–22 on Apple Silicon at the default `Release`
optimization level.

The same constraint already applied to subtests via
`att_subtest_scope_protect` (a macro in `include/attest/attest.h`).

### Windows setjmp Stack Corruption (Resolved)

**Problem:** `STATUS_BAD_STACK (0xC0000028)` when using `setjmp` through function calls.

**Solution:** `setjmp` must be macro-expanded directly into caller's stack frame:

```c
// Wrong: function call wrapping setjmp
int helper(void) { return setjmp(env); }  // env invalid after return

// Correct: macro expansion
#define PROTECT() setjmp(*get_env_ptr())
if (PROTECT() == 0) { ... }  // setjmp in this stack frame
```

This is the same constraint that the cross-frame `setjmp` fix above
extends to the top-level test runner.

### MSVC SCOPED_INFO Limitation

MSVC doesn't support `__attribute__((cleanup))`. `SCOPED_INFO` pushes context but doesn't auto-pop. Stack resets at test end, so functionality is preserved.

---

## Implementation Details

### Test Naming

- Format: `Suite.Name`
- Duplicates detected at initialization → exit code 3
- Stored with file path and line number for error reporting

### Fatal vs Non-Fatal Assertions

| Type | Behavior | Implementation |
|------|----------|----------------|
| `ASSERT_*` | Abort current test | `longjmp` to test's `setjmp` point |
| `EXPECT_*` | Record failure, continue | Increment failure counter |

### String Comparison

- `NULL == NULL` → true
- `NULL` vs non-NULL → false
- Output shows `"(null)"` for NULL pointers

### Floating-Point Comparison

- `NEAR`: `fabs(a - b) <= epsilon`
- `NEAR_REL`: `fabs(a - b) <= rel_eps * max(fabs(a), fabs(b))`
  - Near-zero values (< 1e-15): uses absolute comparison
- `ULP_EQ`: IEEE 754 bit representation distance
  - Handles denormals and sign transitions
- NaN always fails
- ±Infinity must match exactly (same sign)

### Exit Codes

| Code | Condition |
|------|-----------|
| 0 | All tests passed, or `--list` mode |
| 1 | One or more failures |
| 2 | CLI error (unknown option) |
| 3 | Initialization error (duplicate names, internal failure) |

---

## Development Guidelines

### Code Style

- WebKit-based: tabs (width 4), right pointer alignment
- Run `clang-format -i <file>` before committing
- Target 100 columns line length

### Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Public API | `att_` prefix, snake_case | `att_run_subtest` |
| Macros | UPPER_SNAKE_CASE | `ASSERT_EQ` |
| Files | snake_case | `attest_assert.c` |

### Testing Changes

1. Add self-tests in `tests/selftest_main.c`
2. Run full suite: `./build/attest_selftest`
3. Test CLI behavior: `--list`, `--filter=...`
4. Verify on multiple platforms if possible
