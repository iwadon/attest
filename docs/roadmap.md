# Roadmap

This document outlines future plans for attest beyond the current implementation (P0, P1, P1.1).

## Current Status

| Phase | Status | Features |
|-------|--------|----------|
| P0 | Complete | Core assertions, CLI, exit codes, subtests, output capture |
| P1 | Complete | Fixtures, skip API, TAP/JUnit output, timeouts, parallel execution |
| P1.1 | Complete | `NEAR_REL`, `ULP_EQ`, `SCOPED_INFO` |
| P1.2 | Complete | Negative filters (`--filter=-Pattern`) |
| Refactoring | Planned | Internal code quality improvements (R1-R10) |

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

#### R1: Assertion Handler Consolidation

**Location:** `src/attest_assert.c:853-1038`

**Issue:** Five handler functions follow identical patterns:
- `att_handle_compare_signed()`
- `att_handle_compare_unsigned()`
- `att_handle_compare_double()`
- `att_handle_compare_long_double()`
- `att_handle_compare_pointer()`

**Solution:** Extract generic handler macro or template function.

**Impact:** ~200 lines → ~50 lines

- [ ] Design generic handler interface
- [ ] Implement macro-based solution
- [ ] Verify all assertion types still work

#### R2: Comparison Function Unification

**Location:** `src/attest_assert.c:813-987`

**Issue:** Four comparison functions with identical switch statements:
- `att_compare_values()` (signed)
- `att_compare_unsigned_values()`
- `att_compare_double_values()`
- `att_compare_long_double_values()`

**Solution:** Create type-generic comparison macro.

**Impact:** ~80 lines → ~20 lines

- [ ] Define `ATT_DEFINE_COMPARE(type)` macro
- [ ] Replace individual functions
- [ ] Add tests for edge cases

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

#### R4: Platform Timeout Separation

**Location:** `src/attest_assert.c:403-549`

**Issue:** POSIX/Windows/Human68k implementations mixed in ~150 lines of conditional compilation.

**Solution:** Split into platform-specific files:
- `src/internal/attest_timeout_posix.c`
- `src/internal/attest_timeout_win32.c`
- `src/internal/attest_timeout.h` (common interface)

- [ ] Define platform-agnostic interface
- [ ] Extract POSIX implementation
- [ ] Extract Windows implementation
- [ ] Update CMakeLists.txt

#### R5: Split `att_handle_string()`

**Location:** `src/attest_assert.c:1256-1353`

**Issue:** 100-line function with multiple responsibilities:
1. Equality check
2. Color formatting
3. Info stack output
4. Multi-line diff
5. Single-line formatting

**Solution:** Split into focused functions:
- `att_handle_string_diff()` - multi-line comparison
- `att_handle_string_simple()` - single-line comparison

- [ ] Extract multi-line diff logic
- [ ] Extract single-line logic
- [ ] Keep main function as dispatcher

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

#### R9: String Utility Consolidation

**Location:** `src/attest_cli.c:19-44`, `src/attest_assert.c:661`

**Issue:** `att_strdup()`, `att_strndup()`, `att_dup_string()` scattered across files.

**Solution:** Consolidate into `src/internal/attest_string.c` or inline where used once.

- [ ] Audit all string utility usage
- [ ] Consolidate or inline

#### R10: Fixture Cleanup Unification

**Location:** `src/attest_assert.c:301-343`

**Issue:** `att_context_fixture_cleanup()` and `att_context_fixture_on_abort()` share identical cleanup logic.

**Solution:** Extract `att_context_fixture_cleanup_impl(bool run_teardown)`.

- [ ] Extract common implementation
- [ ] Refactor both functions

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
