# Roadmap

This document outlines future plans for attest beyond the current implementation (P0, P1, P1.1).

## Current Status

| Phase | Status | Features |
|-------|--------|----------|
| P0 | Complete | Core assertions, CLI, exit codes, subtests, output capture |
| P1 | Complete | Fixtures, skip API, TAP/JUnit output, timeouts, parallel execution |
| P1.1 | Complete | `NEAR_REL`, `ULP_EQ`, `SCOPED_INFO` |

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

- Negative filters: `--filter=-Slow.*`
- Regex support: `--filter=/Math\\..*Add/`
- Tag-based filtering: `[slow]`, `[integration]`

### Statistics API

Expose internal statistics for programmatic access:

```c
const att_summary* attest_summary(void);
// Contains: total, failed, skipped, execution time, etc.
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
