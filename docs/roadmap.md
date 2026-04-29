# Roadmap

This document outlines future plans for attest beyond the current implementation (P0, P1, P1.1).

## Current Status

| Phase | Status | Features |
|-------|--------|----------|
| P0 | Complete | Core assertions, CLI, exit codes, subtests, output capture |
| P1 | Complete | Fixtures, skip API, TAP/JUnit output, timeouts, parallel execution |
| P1.1 | Complete | `NEAR_REL`, `ULP_EQ`, `SCOPED_INFO` |
| P1.2 | Complete | Negative filters (`--filter=-Pattern`) |

---

## P1+ (Future)

### Type Extensions

- ~~`long double` support~~ (Implemented: `att_handle_compare_long_double` is wired into `_Generic` and exercised by the `LongDouble.*` self-tests)
- `_Decimal64` / `_Float128` (where available)
- ~~C89/90 explicit-type macros: `EXPECT_INT_EQ`, `EXPECT_UINT_EQ`, `EXPECT_PTR_EQ`~~ (Implemented)

### Custom Assertions ✓

Implemented as `ATT_ASSERT(expr, fmt, ...)` and `ATT_EXPECT(expr, fmt, ...)` with printf-style formatting.

```c
ATT_ASSERT(code == 0, "unexpected error code: %d", code);
ATT_EXPECT(is_valid(ptr), "ptr %p is not valid", ptr);
```

### Advanced Filtering

- ~~Negative filters: `--filter=-Slow.*`~~ (Implemented in P1.2)
- Regex support: `--filter=/Math\\..*Add/`
- Tag-based filtering: `[slow]`, `[integration]`

### Statistics API ✓

Implemented as `attest_get_summary()` returning `attest_summary` struct with `total`, `passed`, `failed`, `skipped` fields.

```c
attest_summary s = attest_get_summary();
printf("Ran %d tests, %d passed\n", s.total, s.passed);
```

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

- ~~Explicit-type macros (no `_Generic`)~~ (Implemented: `EXPECT_INT_*`, `EXPECT_UINT_*`, `EXPECT_PTR_*`)
- ~~Manual registration fallback~~ (Implemented: `ATT_REGISTER_TESTS(...)`)
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
