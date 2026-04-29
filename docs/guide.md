# User Guide

This guide covers common usage patterns and advanced features of attest.

## Basic Tests

The simplest test uses the `TEST(Suite, Name)` macro:

```c
#include "attest/attest.h"

TEST(Math, Addition) {
    ASSERT_EQ(2 + 2, 4);
    EXPECT_NE(3 + 3, 7);
}

int main(int argc, char **argv) {
    return attest_main(argc, argv);
}
```

Tests are automatically registered at startup via constructor attributes. No manual test list maintenance required.

---

## Test Fixtures

Fixtures provide shared setup/teardown logic for related tests.

### Defining a Fixture

```c
typedef struct {
    int *buffer;
    size_t size;
} BufferFixture;

ATT_FIXTURE_SETUP(BufferFixture) {
    att_fixture->size = 100;
    att_fixture->buffer = malloc(att_fixture->size * sizeof(int));
}

ATT_FIXTURE_TEARDOWN(BufferFixture) {
    free(att_fixture->buffer);
}

TEST_F(BufferFixture, Initialization) {
    ASSERT_NOT_NULL(att_fixture->buffer);
    ASSERT_EQ(att_fixture->size, 100);
}

TEST_F(BufferFixture, FillWithZeros) {
    memset(att_fixture->buffer, 0, att_fixture->size * sizeof(int));
    EXPECT_EQ(att_fixture->buffer[0], 0);
}
```

### Execution Order

1. Fixture instance allocated
2. Setup function called (if defined)
3. Test body executed
4. Teardown function called (if defined) — **always runs**, even after ASSERT failure or skip
5. Fixture instance freed

### Setup/Teardown Failures

- **Setup failure**: Test body is skipped, marked as `(setup)` failure
- **Teardown failure**: Appended to test results with `(teardown)` tag

---

## Skipping Tests

### Unconditional Skip

```c
TEST(Platform, LinuxOnly) {
    #ifndef __linux__
    ATT_SKIP("Linux only");
    #endif
    // Linux-specific test code
}
```

### Conditional Skip

```c
TEST(Feature, RequiresEnv) {
    ATT_SKIP_IF(getenv("FEATURE_FLAG") == NULL, "FEATURE_FLAG not set");
    // test code
}
```

Skipped tests:
- Don't count as failures
- Show in summary as `[  SKIPPED ]`
- Exit code remains 0 (unless other tests fail)

---

## Scoped Info

Add context that appears only when assertions fail:

```c
TEST(DataDriven, Validation) {
    int test_cases[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        SCOPED_INFO("test_case[%d] = %d", i, test_cases[i]);
        EXPECT_GT(test_cases[i], 0);
    }
}
```

On failure, output includes:

```
[  FAILED  ] DataDriven.Validation
  context: test_case[3] = 4
  file.c:10: EXPECT_GT(...) failed.
```

**Features:**
- Nesting supported (stack depth limit: 8)
- Auto-pops on scope exit (GCC/Clang only)
- Context only printed on failure

---

## Subtests

Run isolated tests that don't affect parent test state:

```c
static void validate_positive(void *user) {
    int value = *(int *)user;
    ASSERT_GT(value, 0);
}

TEST(Validation, MultipleInputs) {
    int values[] = {42, -1, 100};
    att_result result;

    for (int i = 0; i < 3; i++) {
        SCOPED_INFO("value=%d", values[i]);
        att_run_subtest("check", validate_positive, &values[i], &result);

        if (values[i] > 0) {
            EXPECT_EQ(result.status, ATT_STATUS_OK);
        } else {
            EXPECT_EQ(result.status, ATT_STATUS_FAIL);
        }
    }
}
```

### Expected Failures

Validate that code fails as expected:

```c
TEST(ErrorHandling, InvalidInput) {
    ATT_EXPECT_SUBTEST_FAILS("null_check", {
        char *ptr = NULL;
        ASSERT_NOT_NULL(ptr);
    }, 1, 1);  // expect exactly 1 failure
}
```

The macro parameters are:
- `name`: Subtest name for reporting
- `block`: Code block to execute
- `min`: Minimum expected failures
- `max`: Maximum expected failures

---

## Floating-Point Comparisons

### Absolute Error (NEAR)

```c
TEST(Float, AbsoluteError) {
    double computed = 3.14159;
    double expected = 3.14;
    EXPECT_NEAR(computed, expected, 0.01);  // |diff| <= 0.01
}
```

### Relative Error (NEAR_REL)

Better for values of varying magnitude:

```c
TEST(Float, RelativeError) {
    EXPECT_NEAR_REL(100.0, 101.0, 0.02);     // 1% diff, 2% tolerance
    EXPECT_NEAR_REL(1e10, 1e10 + 1e8, 0.02); // works for large values
}
```

### ULP Comparison (ULP_EQ)

For precise floating-point comparison:

```c
TEST(Float, ULPDistance) {
    EXPECT_ULP_EQ(1.0, 1.0, 0);              // exact match
    EXPECT_ULP_EQ(1.0, nextafter(1.0, 2.0), 1);  // 1 ULP apart
    EXPECT_ULP_EQ(0.0, -0.0, 0);             // +0 and -0 are 0 ULPs apart
}
```

---

## Parallel Execution

Run tests in parallel for faster execution:

```bash
# Use 4 worker threads
./test_runner --jobs=4

# Auto-detect CPU cores
./test_runner --jobs=auto
```

**Notes:**
- Tests must be independent (no shared mutable state)
- Output is collected per-test and printed in registration order
- **Format restrictions:** Parallel execution currently emits only the default
  human-readable format. The TAP per-test lines (`ok N` / `not ok N`) and the
  JUnit XML report are not produced when `--jobs > 1`; if you need those
  formats, run with `--jobs=1` (the default) so the sequential path is used.
- **Platform support:** Requires POSIX threads (Linux, macOS). On platforms
  without thread support (e.g., Human68k) or on Windows, `--jobs` silently
  falls back to sequential execution.

---

## CMake Integration

### As a Subproject

```cmake
add_subdirectory(external/attest)

add_executable(my_tests tests/my_tests.c)
target_link_libraries(my_tests PRIVATE attest)
```

Self-tests are automatically disabled when used as a subproject. To enable:

```bash
cmake -S . -B build -DATTEST_BUILD_TESTING=ON
```

### Using FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    attest
    GIT_REPOSITORY https://github.com/user/attest.git
    GIT_TAG main
)
FetchContent_MakeAvailable(attest)

target_link_libraries(my_tests PRIVATE attest)
```

### Bundled `attest_runner`

The top-level CMake build also produces an `attest_runner` executable
(`src/attest_main.c`) that simply calls `attest_main()`. It exists so the
library's link surface can be verified standalone, and as a minimal
"copy this `main()`" example. It contains no tests of its own — link your
own translation units to drive it, or use your own `main()` and skip the
runner entirely.

---

## Output Formats

### Default (Human-Readable)

Use `--no-color` to disable colored output (useful for CI environments or log files).

```bash
./test_runner
```

The default formatter is intentionally terse: passing tests produce no per-test
output. Only failures and skips are reported, followed by a final summary.

```
[==========] Running 10 tests from 3 suites.
[  FAILED  ] Math.Division
  test.c:15: EXPECT_EQ(result, 2) failed.
    expected: 2
      actual: 3
    expr: result=3, 2=2
[  SKIPPED ] Platform.LinuxOnly
  reason: Linux only
[==========] 10 tests ran. 1 failures, 1 skipped.
[  FAILED  ] 1 tests.
[  PASSED  ] 8 tests.
[  SKIPPED ] 1 tests.
```

When tests run in parallel (`--jobs=N` with `N > 1`), each test additionally
emits `[ RUN      ]` / `[       OK ]` / `[  FAILED  ]` markers as it completes,
to make worker progress visible. Sequential runs omit those markers for
brevity.

### TAP 13

```bash
./test_runner --format=tap
```

```
1..10
ok 1 - Math.Addition
not ok 2 - Math.Division
  ---
  message: EXPECT_EQ failed
  ...
```

### JUnit XML

```bash
./test_runner --format=junit --output=results.xml
```

Generates CI-compatible XML for Jenkins, GitHub Actions, etc.

---

## Timeout Handling

Set per-test timeout:

```bash
./test_runner --timeout-ms=5000
```

**Platform differences:**
- **POSIX**: Signal-based interruption (works for infinite loops)
- **Windows**: Polling-based (requires assertion macro execution to detect timeout)

For Windows, ensure long-running code includes periodic assertions:

```c
TEST(LongRunning, WithProgress) {
    for (int i = 0; i < 1000000; i++) {
        // Do work...
        if (i % 10000 == 0) {
            EXPECT_TRUE(true);  // Allows timeout check
        }
    }
}
```

---

## Manual Registration

For compilers without `__attribute__((constructor))`:

```c
// Define tests normally
TEST(Suite, Test1) { ... }
TEST(Suite, Test2) { ... }

// Manually register
ATT_REGISTER_TESTS(
    &test_Suite_Test1_register,
    &test_Suite_Test2_register
);

int main(int argc, char **argv) {
    return attest_main(argc, argv);
}
```

The register function name follows the pattern: `test_<Suite>_<Name>_register`
