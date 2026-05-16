# API Reference

**English** | [日本語](api.ja.md)

This document provides a complete reference for all attest macros, functions, and CLI options.

## Test Definition Macros

### TEST(Suite, Name)

Defines a simple test case.

```c
TEST(Math, Addition) {
    ASSERT_EQ(2 + 2, 4);
}
```

### TEST_F(Fixture, Name)

Defines a test case with a fixture. The fixture type must be a struct, and setup/teardown functions are optional.

```c
typedef struct {
    int value;
} MyFixture;

ATT_FIXTURE_SETUP(MyFixture) {
    att_fixture->value = 42;
}

ATT_FIXTURE_TEARDOWN(MyFixture) {
    // cleanup
}

TEST_F(MyFixture, Example) {
    ASSERT_EQ(att_fixture->value, 42);
}
```

### ATT_FIXTURE_SETUP(Type) / ATT_FIXTURE_TEARDOWN(Type)

Define setup and teardown functions for a fixture. Inside these functions,
`att_fixture` is a pointer to the fixture instance. Both hooks are optional
and registered through constructor attributes — the same `Type` may be reused
across many `TEST_F`s.

The fixture instance is freshly zero-initialized for every `TEST_F` (each
test gets its own copy). Teardown is **always** invoked on exit, even when
the test body aborts via `ASSERT_*`, calls `ATT_SKIP`, or hits a timeout —
this is the documented place to release resources allocated during setup.

---

## Assertions

All assertions come in two variants:
- **ASSERT_*** — Fatal: aborts the current test on failure
- **EXPECT_*** — Non-fatal: records failure and continues

### Comparison Assertions

| Macro | Condition |
|-------|-----------|
| `ASSERT_EQ(a, b)` / `EXPECT_EQ(a, b)` | `a == b` |
| `ASSERT_NE(a, b)` / `EXPECT_NE(a, b)` | `a != b` |
| `ASSERT_LT(a, b)` / `EXPECT_LT(a, b)` | `a < b` |
| `ASSERT_LE(a, b)` / `EXPECT_LE(a, b)` | `a <= b` |
| `ASSERT_GT(a, b)` / `EXPECT_GT(a, b)` | `a > b` |
| `ASSERT_GE(a, b)` / `EXPECT_GE(a, b)` | `a >= b` |

Type dispatch uses C11 `_Generic` to handle signed/unsigned integers, floating-point, and pointers.

### Explicit Type Macros (C89 Compatible)

These macros provide type-explicit assertions that work in C89 environments without `_Generic`.

#### Signed Integer (`INT`)

- `EXPECT_INT_EQ(lhs, rhs)` / `ASSERT_INT_EQ(lhs, rhs)` — Equal
- `EXPECT_INT_NE(lhs, rhs)` / `ASSERT_INT_NE(lhs, rhs)` — Not equal
- `EXPECT_INT_LT(lhs, rhs)` / `ASSERT_INT_LT(lhs, rhs)` — Less than
- `EXPECT_INT_LE(lhs, rhs)` / `ASSERT_INT_LE(lhs, rhs)` — Less than or equal
- `EXPECT_INT_GT(lhs, rhs)` / `ASSERT_INT_GT(lhs, rhs)` — Greater than
- `EXPECT_INT_GE(lhs, rhs)` / `ASSERT_INT_GE(lhs, rhs)` — Greater than or equal

Values are cast to `long long` for comparison.

```c
TEST(Numbers, SignedInteger) {
    int a = 42;
    int b = 10;
    ASSERT_INT_EQ(a, 42);
    EXPECT_INT_GT(a, b);
}
```

#### Unsigned Integer (`UINT`)

- `EXPECT_UINT_EQ(lhs, rhs)` / `ASSERT_UINT_EQ(lhs, rhs)` — Equal
- `EXPECT_UINT_NE(lhs, rhs)` / `ASSERT_UINT_NE(lhs, rhs)` — Not equal
- `EXPECT_UINT_LT(lhs, rhs)` / `ASSERT_UINT_LT(lhs, rhs)` — Less than
- `EXPECT_UINT_LE(lhs, rhs)` / `ASSERT_UINT_LE(lhs, rhs)` — Less than or equal
- `EXPECT_UINT_GT(lhs, rhs)` / `ASSERT_UINT_GT(lhs, rhs)` — Greater than
- `EXPECT_UINT_GE(lhs, rhs)` / `ASSERT_UINT_GE(lhs, rhs)` — Greater than or equal

Values are cast to `unsigned long long` for comparison.

```c
TEST(Numbers, UnsignedInteger) {
    unsigned int size = 256;
    ASSERT_UINT_EQ(size, 256u);
    EXPECT_UINT_GT(size, 100u);
}
```

#### Pointer (`PTR`)

- `EXPECT_PTR_EQ(lhs, rhs)` / `ASSERT_PTR_EQ(lhs, rhs)` — Equal
- `EXPECT_PTR_NE(lhs, rhs)` / `ASSERT_PTR_NE(lhs, rhs)` — Not equal
- `EXPECT_PTR_LT(lhs, rhs)` / `ASSERT_PTR_LT(lhs, rhs)` — Less than
- `EXPECT_PTR_LE(lhs, rhs)` / `ASSERT_PTR_LE(lhs, rhs)` — Less than or equal
- `EXPECT_PTR_GT(lhs, rhs)` / `ASSERT_PTR_GT(lhs, rhs)` — Greater than
- `EXPECT_PTR_GE(lhs, rhs)` / `ASSERT_PTR_GE(lhs, rhs)` — Greater than or equal

Pointers are compared as `uintptr_t` values.

```c
TEST(Pointer, Comparison) {
    int arr[10];
    int *p1 = &arr[0];
    int *p2 = &arr[5];
    ASSERT_PTR_NE(p1, p2);
    EXPECT_PTR_LT(p1, p2);
}
```

### Boolean Assertions

| Macro | Condition |
|-------|-----------|
| `ASSERT_TRUE(cond)` / `EXPECT_TRUE(cond)` | `cond` is true |
| `ASSERT_FALSE(cond)` / `EXPECT_FALSE(cond)` | `cond` is false |

### Pointer Assertions

| Macro | Condition |
|-------|-----------|
| `ASSERT_NULL(ptr)` / `EXPECT_NULL(ptr)` | `ptr == NULL` |
| `ASSERT_NOT_NULL(ptr)` / `EXPECT_NOT_NULL(ptr)` | `ptr != NULL` |

### String Assertions

| Macro | Condition |
|-------|-----------|
| `ASSERT_STREQ(s1, s2)` / `EXPECT_STREQ(s1, s2)` | `strcmp(s1, s2) == 0` |
| `ASSERT_STRNE(s1, s2)` / `EXPECT_STRNE(s1, s2)` | `strcmp(s1, s2) != 0` |

- `NULL == NULL` is true
- `NULL` vs non-NULL is false
- Output shows `"(null)"` for NULL pointers

### Memory Assertions

| Macro | Condition |
|-------|-----------|
| `ASSERT_MEMEQ(p1, p2, n)` / `EXPECT_MEMEQ(p1, p2, n)` | `memcmp(p1, p2, n) == 0` |

- `n == 0` always succeeds
- NULL pointer with `n > 0` fails

### Floating-Point Assertions

| Macro | Condition |
|-------|-----------|
| `ASSERT_NEAR(a, b, eps)` / `EXPECT_NEAR(a, b, eps)` | `|a - b| <= eps` |
| `ASSERT_NEAR_REL(a, b, rel)` / `EXPECT_NEAR_REL(a, b, rel)` | `|a - b| <= rel * max(|a|, |b|)` |
| `ASSERT_ULP_EQ(a, b, ulp)` / `EXPECT_ULP_EQ(a, b, ulp)` | ULP distance <= ulp |

**Special cases:**
- NaN always fails
- ±Infinity must match exactly (same sign)
- `NEAR_REL`: when both `|lhs|` and `|rhs|` are below `1e-15`, the comparison
  switches to an absolute test using `rel_eps` itself as the absolute
  tolerance (i.e. `|lhs - rhs| <= rel_eps`). This avoids dividing by an
  effectively-zero magnitude.
- `ULP_EQ`: handles denormals and sign transitions correctly

### Custom Assertions

| Macro | Description |
|-------|-------------|
| `ATT_ASSERT(expr, fmt, ...)` | Fatal: custom message with printf-style formatting |
| `ATT_EXPECT(expr, fmt, ...)` | Non-fatal: custom message with printf-style formatting |

```c
TEST(Validation, Custom) {
    int code = get_error_code();
    ATT_ASSERT(code == 0, "unexpected error code: %d", code);
}
```

---

## Test Control

### ATT_SKIP(reason)

Skip the current test with a reason message.

```c
TEST(Platform, LinuxOnly) {
    #ifndef __linux__
    ATT_SKIP("Linux only");
    #endif
    // test code
}
```

### ATT_SKIP_IF(condition, reason)

Skip the current test if condition is true.

```c
TEST(Feature, RequiresEnv) {
    ATT_SKIP_IF(getenv("CI") != NULL, "Skipped in CI");
    // test code
}
```

### SCOPED_INFO(fmt, ...)

Add contextual information that appears only on failure. Uses GCC/Clang `cleanup` attribute for automatic scope management.

```c
TEST(Loop, Example) {
    for (int i = 0; i < 100; i++) {
        SCOPED_INFO("iteration=%d", i);
        EXPECT_GT(compute(i), 0);
    }
}
```

**Note:** MSVC does not support `cleanup` attribute; info is pushed but not auto-popped. Functionally equivalent as stack resets at test end.

---

## Subtest API

### att_run_subtest(name, fn, user, result)

Run an isolated sub-test that doesn't affect the parent test.

```c
static void my_subtest(void *user) {
    int val = *(int *)user;
    ASSERT_GT(val, 0);
}

TEST(Validation, Subtest) {
    int value = 42;
    att_result result;
    att_status status = att_run_subtest("check_positive", my_subtest, &value, &result);
    EXPECT_EQ(status, ATT_STATUS_OK);
    EXPECT_EQ(result.failed, 0);
}
```

**Signature:**

```c
att_status att_run_subtest(const char *name,
                           void (*fn)(void *), void *user,
                           att_result *out);
```

The return value mirrors `out->status`, so either form is fine. `out` may be
non-NULL to receive detailed counters; pass `NULL` if only the status is
needed.

### ATT_EXPECT_SUBTEST_FAILS(name, block, min, max)

Validate that a code block produces expected failures.

```c
TEST(Error, ExpectedFailure) {
    ATT_EXPECT_SUBTEST_FAILS("should_fail", {
        ASSERT_TRUE(false);
    }, 1, 1);  // expect exactly 1 failure
}
```

### Subtest Scope API

For fine-grained control over subtest execution:

```c
TEST(Scope, Example) {
    att_subtest_scope *scope = att_subtest_scope_enter("my_scope");
    if (att_subtest_scope_protect(scope) == 0) {
        ASSERT_TRUE(some_condition());
    }
    att_result result;
    att_subtest_scope_leave(scope, &result);
    EXPECT_EQ(result.status, ATT_STATUS_OK);
}
```

- `att_subtest_scope_enter(name)` — creates and returns a subtest scope
- `att_subtest_scope_protect(scope)` — macro that calls `setjmp` in the caller's stack frame for fatal assertion handling
- `att_subtest_scope_leave(scope, result)` — cleans up scope and populates result

---

## Output Capture

### att_capture_begin() / att_capture_end(out)

Capture stderr output for validation. Non-reentrant (no nesting) and not
thread-safe — the implementation uses a single global capture state, so
under `--jobs=N` keep each begin/end pair inside one test body on one
worker. Both functions return `0` on success and `-1` on error or when
capture is unsupported on the current platform.

```c
att_captured cap;
att_capture_begin();
fprintf(stderr, "test output");
att_capture_end(&cap);
// cap.data contains "test output", cap.size is length
free(cap.data);
```

**Platform support:** capture relies on `dup`/`dup2`. On Human68k those APIs
are unavailable, so `att_capture_begin()` and `att_capture_end()` are
compiled as no-ops that return `-1`. Tests that depend on
`ATT_EXPECT_SUBTEST_FAILS` (which uses capture internally to silence
expected failure noise) still validate the failure counts on Human68k, but
the captured-output replay step is skipped.

---

## Manual Registration

For compilers without constructor attribute support:

```c
ATT_REGISTER_TESTS(
    &test_Suite_Name_register,
    &test_Other_Test_register
);
```

---

## Types

### att_status

Return type for subtest operations.

```c
typedef enum {
    ATT_STATUS_OK = 0,
    ATT_STATUS_FAIL = 1,
    ATT_STATUS_ABORTED = 2
} att_status;
```

### att_result

Result structure populated by `att_run_subtest()`.

```c
typedef struct att_result {
    int total;
    int failed;
    int fatal_failures;
    int nonfatal_failures;
    int skipped;
    att_status status;
} att_result;
```

### attest_summary

Summary returned by `attest_get_summary()`.

```c
typedef struct attest_summary {
    int total;
    int passed;
    int failed;
    int skipped;
} attest_summary;
```

---

## Entry Point

### attest_main(argc, argv)

Main entry point. Returns exit code.

```c
int main(int argc, char **argv) {
    return attest_main(argc, argv);
}
```

### attest_get_summary()

Returns test execution summary after `attest_main()` completes.

```c
int main(int argc, char **argv) {
    int ret = attest_main(argc, argv);
    attest_summary s = attest_get_summary();
    printf("Ran %d tests, %d passed\n", s.total, s.passed);
    return ret;
}
```

---

## Command-Line Options

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Show usage information and exit |
| `--list` | List all test names and exit |
| `--filter=<pattern>` | Run only matching tests |
| `--no-color` | Disable colored output |
| `--timeout-ms=<ms>` | Set per-test timeout in milliseconds (POSIX: signal-based; Windows: polled at every assertion) |
| `--shuffle` | Randomize test execution order (seed derived from `time(NULL)`) |
| `--shuffle=<seed>` | Shuffle with specific seed for reproducibility |
| `--jobs=<N>` | Run tests in parallel with N workers (`--jobs=1` keeps the sequential runner) |
| `--jobs=auto`, `--jobs=0` | Use detected CPU core count for parallelism |
| `--format=default` | Human-readable output (the default) |
| `--format=tap` | Output in TAP 13 format |
| `--format=junit` | Output in JUnit XML format |
| `--output=<path>` | Write JUnit XML to file (default: `test_detail.xml`; requires `--format=junit`) |

**Parallel runner caveats:** parallel execution is compiled in only on
POSIX-thread platforms (Linux, macOS). On Windows or Human68k, `--jobs > 1`
silently falls back to the sequential runner. Even on POSIX, the parallel
runner currently emits only the default human-readable format — combining it
with `--format=tap` or `--format=junit` suppresses per-test output and skips
the JUnit XML write, so use `--jobs=1` (the default) when you need those
formats.

### Filter Syntax

- `Suite.Name` — exact match
- `Suite.*` — all tests in suite
- `*.Name` — test name in any suite
- `Suite` — shorthand for `Suite.*`
- `.Name` — shorthand for `*.Name`
- `*`, `?` — wildcards
- `Pattern1;Pattern2` — multiple patterns (OR logic)
- `-Pattern` — negative filter: exclude matching tests
- `Pattern1;-Pattern2` — combine inclusions and exclusions (run Pattern1 except Pattern2)

**Examples:**
- `--filter=Math.*` — run all tests in Math suite
- `--filter='-Slow*'` — run all tests except those starting with "Slow"
- `--filter='Math.*;-Math.Slow*'` — run all Math tests except Math.Slow* tests
- `--filter='*.Fast'` — run Fast tests from any suite
- `--filter='*.Fast;-Skip.*'` — run Fast tests except Skip suite

---

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | All tests passed (or `--list` mode) |
| 1 | One or more test failures |
| 2 | CLI parsing error |
| 3 | Initialization failure (duplicate names, internal error) |
