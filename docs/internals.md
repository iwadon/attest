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
  - `att_handle_compare_signed` — signed integers
  - `att_handle_compare_unsigned` — unsigned integers
  - `att_handle_compare_double` — floating-point
  - `att_handle_compare_pointer` — pointers
- Context state tracks: current test, failure counts, longjmp buffer, timeout state, info stack

### CLI Parser

**File:** `src/attest_cli.c`

- Parses `--list`, `--filter`, `--no-color`, `--timeout-ms`, `--shuffle`, `--jobs`, `--format`, `--output`
- Filter syntax: wildcards (`*`, `?`), shorthand (`Suite` → `Suite.*`), multiple patterns (`;` separator)
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

- Spawns timer thread with `_beginthreadex()`
- Uses `CreateEvent()` for timeout notification
- Main thread polls `WaitForSingleObject()` in `att_context_record_assert()`, throttled to every 32 assertions for performance
- **Limitation:** Infinite loops without assertions won't be interrupted

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

### GCC 14.2.0 ARM64 sigsetjmp Bug

**Platform:** ARM64/aarch64 (Ubuntu 25.04)
**Symptom:** SIGILL crash when running full test suite
**Cause:** GCC 14.2.0 ARM64 backend corrupts x30 register in `sigsetjmp/siglongjmp`

**Workarounds:**
1. Use Clang on ARM64 (recommended)
2. Use GCC 13.x
3. Applied mitigations in code:
   - 16-byte aligned `sigjmp_buf` with `volatile` qualifier
   - `-fno-omit-frame-pointer` and `-fno-optimize-sibling-calls`
   - `-O1` for GCC 14.x on ARM64

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
