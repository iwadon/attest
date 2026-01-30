# Roadmap

This document outlines future plans for attest beyond the current implementation (P0, P1, P1.1).

## Current Status

| Phase | Status | Features |
|-------|--------|----------|
| P0 | Complete | Core assertions, CLI, exit codes, subtests, output capture |
| P1 | Complete | Fixtures, skip API, TAP/JUnit output, timeouts, parallel execution |
| P1.1 | Complete | `NEAR_REL`, `ULP_EQ`, `SCOPED_INFO` |
| P1.2 | Complete | Negative filters (`--filter=-Pattern`) |
| Refactoring | Complete | Internal code quality improvements (R1-R10) |

---

## P1+ (Future)

### Type Extensions

- `long double` support
- `_Decimal64` / `_Float128` (where available)
- C89/90 explicit-type macros: `EXPECT_INT_EQ`, `EXPECT_UINT_EQ`, `EXPECT_PTR_EQ`

### Custom Assertions

```c
// Simple boolean assertion with custom message
ATT_ASSERT(expr, "custom message");

// Predicate-based assertion
ATT_EXPECT(predicate_fn, context);
```

### Advanced Filtering

- ~~Negative filters: `--filter=-Slow.*`~~ (Implemented in P1.2)
- Regex support: `--filter=/Math\\..*Add/`
- Tag-based filtering: `[slow]`, `[integration]`

### Statistics API

Expose internal statistics for programmatic access:

```c
const att_summary* attest_summary(void);
// Contains: total, failed, skipped, execution time, etc.
```

---

## Refactoring (Technical Debt)

Internal code quality improvements that don't change public API.

### High Priority (Major code reduction)

#### R1: Assertion Handler Consolidation ✓

**Location:** `src/attest_assert.c:844-872`

**Issue:** Five handler functions follow identical patterns:
- `att_handle_compare_signed()`
- `att_handle_compare_unsigned()`
- `att_handle_compare_double()`
- `att_handle_compare_long_double()`
- `att_handle_compare_pointer()`

**Solution:** Extract generic handler macro or template function.

**Status:** Complete

- [x] Design generic handler interface
- [x] Implement `ATT_DEFINE_HANDLER(name, type, compare_fn, format_fn)` macro
- [x] Replace signed, unsigned, double, long_double with macro invocations
- [x] Keep pointer handler as-is (requires `void*` to `uintptr_t` conversion)
- [x] Existing tests cover all assertion types

#### R2: Comparison Function Unification ✓

**Location:** `src/attest_assert.c:813-987`

**Issue:** Four comparison functions with identical switch statements:
- `att_compare_values()` (signed)
- `att_compare_unsigned_values()`
- `att_compare_double_values()`
- `att_compare_long_double_values()`

**Solution:** Create type-generic comparison macro.

**Status:** Complete

- [x] Define `ATT_DEFINE_COMPARE(name, type)` macro
- [x] Replace all four functions with macro invocations
- [x] Existing tests cover edge cases

#### R3: Failure Reporting Extraction ✓

**Location:** `src/attest_assert.c:736-811, 1256-1353`

**Issue:** Info stack printing loop duplicated 4 times across `att_report_failure()` and `att_handle_string()`.

**Solution:** Extract `att_failure_context` struct and helper functions.

**Status:** Complete

- [x] Create `att_failure_context` struct and `att_failure_begin()` helper
- [x] Create `att_print_info_stack_and_location()` helper
- [x] Refactor `att_report_failure()`
- [x] Refactor `att_handle_string()`

### Medium Priority (Maintainability)

#### R4: Platform Timeout Separation ✓

**Location:** `src/attest_assert.c:403-549` (original)

**Issue:** POSIX/Windows/Human68k implementations mixed in ~150 lines of conditional compilation.

**Solution:** Split into platform-specific files:
- `src/internal/attest_timeout_posix.c`
- `src/internal/attest_timeout_win32.c`
- `src/internal/attest_timeout.h` (common interface)

**Status:** Complete

- [x] Define platform-agnostic interface (`att_timeout_start`, `att_timeout_stop`)
- [x] Extract POSIX implementation (includes Human68k stubs)
- [x] Extract Windows implementation
- [x] Update CMakeLists.txt (conditional source selection)
- [x] Add context accessor functions to bridge timeout modules with context state

#### R5: Split `att_handle_string()` ✓

**Location:** `src/attest_assert.c:1041-1101` (original: 1256-1353, reduced by R3)

**Issue:** Function with multiple responsibilities:
1. Equality check
2. Info stack output
3. Multi-line diff
4. Single-line formatting

**Solution:** Split into focused functions:
- `att_handle_string_diff()` - multi-line comparison
- `att_handle_string_simple()` - single-line comparison

**Status:** Complete

- [x] Extract multi-line diff logic
- [x] Extract single-line logic
- [x] Keep main function as dispatcher

#### R6: CPU Detection Helper ✓

**Location:** `src/attest_cli.c:61-73`

**Issue:** Identical CPU detection code duplicated for `--jobs=auto` and `--jobs=0`.

**Solution:** Extract `att_get_cpu_count()` helper.

**Status:** Complete

- [x] Create helper function
- [x] Replace duplicated code

### Low Priority (Code Simplification)

#### R7: Color Function Lookup Table ✓

**Location:** `src/attest_assert.c:550-583`

**Issue:** Seven color functions with identical conditional pattern.

**Solution:** Use lookup table or macro generation.

**Status:** Complete

- [x] Define color enum
- [x] Create lookup table
- [x] Replace individual functions (kept as thin wrappers for compatibility)

#### R8: Format Function Consolidation ✓

**Location:** `src/attest_assert.c:590-654`

**Issue:** Seven formatting functions (`att_format_signed`, `att_format_unsigned`, etc.) follow same pattern.

**Solution:** Macro-based generation for simple snprintf functions.

**Status:** Complete

- [x] Design `ATT_DEFINE_FORMATTER(name, type, fmt_spec)` macro
- [x] Replace signed, unsigned, double, long_double with macro
- [x] Keep pointer, bool, string as-is (special handling required)

#### R9: String Utility Consolidation — Not needed

**Location:** `src/attest_cli.c:19-44`, `src/attest_assert.c:568`

**Issue:** `att_strdup()`, `att_strndup()`, `att_dup_string()` scattered across files.

**Analysis:** Functions are `static` and isolated to their respective files. Consolidation would add header dependencies without meaningful benefit. Current design allows per-file link-time optimization.

**Decision:** No action needed.

#### R10: Fixture Cleanup Unification — Not needed

**Location:** `src/attest_assert.c:361-403`

**Issue:** `att_context_fixture_cleanup()` and `att_context_fixture_on_abort()` share cleanup logic.

**Analysis:** Only ~8 lines of actual duplication (state reset). The `_on_abort()` variant intentionally copies to local variables before calling teardown for safety after `longjmp`. Unifying would risk subtle bugs for minimal gain.

**Decision:** No action needed.

---

## P2 (Long-term)

### JSON Output

```bash
./test_runner --format=json --output=results.json
```

### Parameterized Tests

```c
ATT_PARAM_TEST(Math, Addition, int, a, int, b, int, expected) {
    ASSERT_EQ(a + b, expected);
}

ATT_PARAM_VALUES(Math, Addition,
    {1, 2, 3},
    {0, 0, 0},
    {-1, 1, 0}
);
```

### Lightweight Benchmarking

```c
ATT_BENCHMARK(Sort, QuickSort) {
    int data[1000];
    // setup...
    ATT_BENCH_START();
    quicksort(data, 1000);
    ATT_BENCH_END();
}
```

### Mock/Stub Helpers

```c
ATT_MOCK(file_read, char*, const char* path) {
    return "mocked content";
}
```

### IDE Integration

- Visual Studio plugin for test discovery
- CLion run configuration
- Output format extensions for better IDE parsing

---

## Compatibility Goals

### C89/90 Support

For embedded/legacy environments without C11:

- Explicit-type macros (no `_Generic`)
- Manual registration fallback
- Optional header-only mode

### Minimal Footprint

- Single-header option for simple integration
- Configurable feature set via compile flags
- Reduced memory allocation for embedded use

---

## Contributing Ideas

Feature requests and implementation ideas are welcome. When proposing new features, consider:

1. **Compatibility**: Does it work across all supported platforms?
2. **Simplicity**: Does it fit attest's lightweight philosophy?
3. **Testing**: Can the feature be self-tested?

For implementation, see [internals.md](internals.md) for architecture details.
