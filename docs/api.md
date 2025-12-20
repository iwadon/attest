# API Reference

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

Define setup and teardown functions for a fixture. Inside these functions, `att_fixture` is a pointer to the fixture instance.

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
- `NEAR_REL`: values near zero (< 1e-15) use absolute comparison
- `ULP_EQ`: handles denormals and sign transitions correctly

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
    att_run_subtest("check_positive", my_subtest, &value, &result);
    EXPECT_EQ(result.failed, 0);
}
```

### ATT_EXPECT_SUBTEST_FAILS(name, block, min, max)

Validate that a code block produces expected failures.

```c
TEST(Error, ExpectedFailure) {
    ATT_EXPECT_SUBTEST_FAILS("should_fail", {
        ASSERT_TRUE(false);
    }, 1, 1);  // expect exactly 1 failure
}
```

---

## Output Capture

### att_capture_begin() / att_capture_end(out)

Capture stderr output for validation. Non-reentrant (no nesting).

```c
att_captured cap;
att_capture_begin();
fprintf(stderr, "test output");
att_capture_end(&cap);
// cap.data contains "test output", cap.size is length
free(cap.data);
```

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

## Entry Point

### attest_main(argc, argv)

Main entry point. Returns exit code.

```c
int main(int argc, char **argv) {
    return attest_main(argc, argv);
}
```

---

## Command-Line Options

| Option | Description |
|--------|-------------|
| `--list` | List all test names and exit |
| `--filter=<pattern>` | Run only matching tests |
| `--no-color` | Disable colored output |
| `--timeout-ms=<ms>` | Set per-test timeout in milliseconds |
| `--shuffle` | Randomize test execution order |
| `--shuffle=<seed>` | Shuffle with specific seed for reproducibility |
| `--jobs=<N>` | Run tests in parallel with N workers |
| `--jobs=auto` | Use CPU core count for parallelism |
| `--format=tap` | Output in TAP 13 format |
| `--format=junit` | Output in JUnit XML format |
| `--output=<path>` | Write JUnit XML to file (default: `test_detail.xml`) |

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
