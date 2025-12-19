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

---

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | All tests passed (or `--list` mode) |
| 1 | One or more test failures |
| 2 | CLI parsing error |
| 3 | Initialization failure (duplicate names, internal error) |
