# attest

A lightweight C11 unit testing framework inspired by GoogleTest, designed for simplicity and automatic test registration.

## Features

- **Simple API**: GoogleTest-inspired syntax with `TEST()` and `TEST_F()` macros
- **Automatic Registration**: Tests register themselves at startup (no manual test list maintenance)
- **Type-Generic Assertions**: Uses C11 `_Generic` for type-safe comparisons
- **Rich Assertions**: Comprehensive set of comparison, string, memory, and floating-point assertions
- **Test Fixtures**: Setup/teardown hooks for test isolation
- **Fatal and Non-Fatal**: `ASSERT_*` aborts the test, `EXPECT_*` continues after failure
- **Subtests**: Isolated sub-test execution with expected failure validation
- **CLI Filtering**: Flexible wildcard-based test filtering
- **Colorized Output**: Clear, readable test results (with `--no-color` option)
- **Skip Support**: Conditional test skipping with `ATT_SKIP()` and `ATT_SKIP_IF()`
- **Scoped Info**: Context messages that appear only on failure
- **Zero Dependencies**: Pure C11 with no external libraries

## Quick Start

### Building

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# Run tests
./build/attest_selftest
```

### Writing Your First Test

```c
#include "attest/attest.h"

TEST(Math, Addition) {
    ASSERT_EQ(2 + 2, 4);
    EXPECT_NE(3 + 3, 7);
}

TEST(String, Comparison) {
    ASSERT_STREQ("hello", "hello");
    EXPECT_STRNE("world", "WORLD");
}

int main(int argc, char **argv) {
    return attest_main(argc, argv);
}
```

### Using Fixtures

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
    ASSERT_NE(att_fixture->buffer, NULL);
    ASSERT_EQ(att_fixture->size, 100);
}
```

## Assertions

### Comparison Assertions

| Macro | Fatal | Description |
|-------|-------|-------------|
| `ASSERT_EQ(a, b)` | Yes | `a == b` |
| `EXPECT_EQ(a, b)` | No | `a == b` |
| `ASSERT_NE(a, b)` | Yes | `a != b` |
| `EXPECT_NE(a, b)` | No | `a != b` |
| `ASSERT_LT(a, b)` | Yes | `a < b` |
| `EXPECT_LT(a, b)` | No | `a < b` |
| `ASSERT_LE(a, b)` | Yes | `a <= b` |
| `EXPECT_LE(a, b)` | No | `a <= b` |
| `ASSERT_GT(a, b)` | Yes | `a > b` |
| `EXPECT_GT(a, b)` | No | `a > b` |
| `ASSERT_GE(a, b)` | Yes | `a >= b` |
| `EXPECT_GE(a, b)` | No | `a >= b` |

### Boolean Assertions

- `ASSERT_TRUE(condition)` / `EXPECT_TRUE(condition)`
- `ASSERT_FALSE(condition)` / `EXPECT_FALSE(condition)`

### String Assertions

- `ASSERT_STREQ(str1, str2)` / `EXPECT_STREQ(str1, str2)` - String equality
- `ASSERT_STRNE(str1, str2)` / `EXPECT_STRNE(str1, str2)` - String inequality

### Memory Assertions

- `ASSERT_MEMEQ(ptr1, ptr2, size)` / `EXPECT_MEMEQ(ptr1, ptr2, size)` - Memory equality

### Floating-Point Assertions

- `ASSERT_NEAR(a, b, epsilon)` / `EXPECT_NEAR(a, b, epsilon)` - Floating-point comparison with tolerance

## Command-Line Options

```bash
# List all tests
./build/attest_selftest --list

# Run specific tests (wildcard support)
./build/attest_selftest --filter='Math.*'
./build/attest_selftest --filter='*.Addition'

# Multiple filters (OR logic)
./build/attest_selftest --filter='Math.*;String.Comparison'

# Disable color output
./build/attest_selftest --no-color
```

## Advanced Features

### Subtests

Run isolated sub-tests that don't affect the parent test:

```c
static void validate_input(void *user) {
    int value = *(int *)user;
    ASSERT_GT(value, 0);
}

TEST(Validation, SubtestExample) {
    int positive = 42;
    int negative = -1;

    att_result result;
    att_run_subtest("positive", validate_input, &positive, &result);
    EXPECT_EQ(result.failed, 0);

    att_run_subtest("negative", validate_input, &negative, &result);
    EXPECT_EQ(result.failed, 1);
}
```

### Expected Failures

Validate that specific code blocks fail as expected:

```c
TEST(ErrorHandling, ExpectedFailure) {
    ATT_EXPECT_SUBTEST_FAILS("invalid_input", {
        att_subtest_fatal(NULL);
    }, 1, 1);  // Expect exactly 1 failure
}
```

### Test Skipping

```c
TEST(Platform, SkipExample) {
    ATT_SKIP_IF(getenv("CI") != NULL, "Skipped in CI environment");
    // Test code that shouldn't run in CI
}
```

### Scoped Info

Add contextual information that appears only when a test fails:

```c
TEST(Loop, ScopedInfo) {
    for (int i = 0; i < 100; i++) {
        SCOPED_INFO("Iteration: %d", i);
        EXPECT_LT(i, 100);
    }
}
```

## Integration

### As a Subproject

Add attest to your CMake project:

```cmake
add_subdirectory(external/attest)

add_executable(my_tests tests/my_tests.c)
target_link_libraries(my_tests PRIVATE attest)
```

By default, attest's self-tests are disabled when used as a subproject. Enable them with:

```bash
cmake -S . -B build -DATTEST_BUILD_TESTING=ON
```

## Exit Codes

- **0**: All tests passed or `--list` mode
- **1**: One or more test failures
- **2**: CLI parsing error (unknown option)
- **3**: Initialization failure (duplicate test names, internal errors)

## Compiler Support

- **GCC**: 5.0+ (C11 support required)
  - ⚠️ **Known Issue**: GCC 14.2.0 on ARM64/aarch64 (Ubuntu 25.04) has a `sigsetjmp/siglongjmp` bug causing crashes. **Workaround**: Use Clang or GCC 13.x on ARM64 platforms.
- **Clang**: 3.1+ (C11 support required) - **Recommended for ARM64**
- **MSVC**: 2015+ (partial support, uses `.CRT$XCU` section for auto-registration)

### Known Issues

- **ARM64 Linux with GCC 14.2.0**: A compiler-specific issue with `sigjmp_buf`/`setjmp`/`longjmp` causes runtime crashes. Use Clang as a workaround:
  ```bash
  cmake -S . -B build -DCMAKE_C_COMPILER=clang
  ```

For compilers without constructor attribute support, use manual registration:

```c
ATT_REGISTER_TESTS(
    &test_register_fn1,
    &test_register_fn2
);
```

## Project Structure

```
attest/
├── include/attest/        # Public API headers
├── src/                   # Implementation
│   ├── attest.c          # Main execution engine
│   ├── attest_cli.c      # Command-line parsing
│   ├── attest_assert.c   # Assertion implementations
│   ├── attest_capture.c  # Output capture
│   ├── attest_fixture.c  # Fixture support
│   └── internal/         # Internal registry
├── tests/                 # Self-tests
├── docs/                  # Specifications (P0, P1, P1+)
└── CMakeLists.txt        # Build configuration
```

## Roadmap

The project follows a phased development approach:

- **P0 (Current)**: Core functionality - C11, single-threaded, basic assertions, CLI
- **P1 (Planned)**: TAP/JUnit output, test timeouts, advanced fixtures
- **P1+ (Future)**: C89/90 support, parallel execution, advanced filtering

See `docs/` directory for detailed specifications.

## Contributing

1. Follow the WebKit code style (tabs, width 4, right pointer alignment)
2. Run `clang-format -i <file>` before committing
3. Add self-tests in `tests/selftest_main.c` for new features
4. Run the full test suite: `./build/attest_selftest`
5. Follow Conventional Commits for commit messages

## License

See `LICENSE` file for details.

## Acknowledgments

Inspired by GoogleTest's ergonomic API design while maintaining a minimal, single-file-friendly implementation suitable for embedded and resource-constrained environments.
