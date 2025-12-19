# attest

**English** | [日本語](README.ja.md)

A lightweight C11 unit testing framework inspired by GoogleTest, designed for simplicity and automatic test registration.

## Features

- GoogleTest-inspired `TEST()` and `TEST_F()` macros
- Automatic test registration (no manual test list maintenance)
- Type-generic assertions using C11 `_Generic`
- Fatal (`ASSERT_*`) and non-fatal (`EXPECT_*`) assertions
- Test fixtures, subtests, skipping, and scoped info
- CLI filtering, colorized output, TAP/JUnit export
- Parallel execution with `--jobs=N`
- Zero external dependencies

## Quick Start

```bash
# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Run tests
./build/attest_selftest
```

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

## Documentation

- **[API Reference](docs/api.md)** — All assertions, macros, and CLI options
- **[User Guide](docs/guide.md)** — Fixtures, subtests, skipping, scoped info, and more
- **[Internals](docs/internals.md)** — Architecture, design decisions, platform support
- **[Roadmap](docs/roadmap.md)** — Future plans (P1+)

## Compiler Support

| Compiler | Version | Notes |
|----------|---------|-------|
| GCC | 5.0+ | ⚠️ GCC 14.2.0 ARM64 has known issues; use Clang |
| Clang | 3.1+ | Recommended for ARM64 |
| MSVC | 2015+ | Uses `.CRT$XCU` for auto-registration |

## License

See [LICENSE](LICENSE) file for details.
