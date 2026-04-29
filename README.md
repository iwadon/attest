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

attest is C99 by default and uses C11 `_Generic` opportunistically (gated by `__STDC_VERSION__`).

| Compiler | Minimum | Notes |
|----------|---------|-------|
| GCC | 5.0+ | Requires `__attribute__((constructor))` |
| Clang | 3.1+ | Same |
| MSVC | 2015+ | Uses `.CRT$XCU` for auto-registration |

### Verified platforms

#### Apple Silicon, macOS 26

The matrix below was verified against the self-test suite (74 tests) on
`arm64-apple-darwin` with `Release` builds (`-O3`).

| Compiler | Version | Status | Notes |
|----------|---------|--------|-------|
| Apple Clang | 21 (Xcode-shipped) | ✅ Pass | Reference toolchain |
| Homebrew Clang | 16 / 17 / 18 / 19 / 20 / 21 / 22 | ✅ Pass | |
| Homebrew Clang | 14 / 15 | ⚠️ Link failure | Homebrew bottles pin `--syslibroot=…/CommandLineTools/SDKs/MacOSX14.sdk`. Update Command Line Tools or pass `-Wl,-syslibroot,$(xcrun --show-sdk-path)`. Compiler itself is fine. |
| Homebrew GCC | 12 / 13 / 14 / 15 | ✅ Pass | |
| Homebrew GCC | 11 | ⚠️ Link failure | Same SDK pinning issue as Clang 14/15. |

Earlier releases of attest needed an `-O1` workaround on ARM64 GCC 14.2 (a
sigsetjmp/longjmp miscompilation). The root cause was actually a cross-frame
`setjmp` in attest itself, which is now fixed by macro-expanding the
test-runner `setjmp` directly into the caller's stack frame. No optimization
flag overrides are required on any of the verified toolchains.

#### Ubuntu 26.04 (aarch64)

Verified on a Parallels Desktop VM with both `Debug` and `Release` builds.
`attest_selftest` reports 74 tests (73 passed / 1 skipped) and
`attest_selftest_c99` reports 12 tests (11 passed / 1 skipped).

| Compiler | Version | Status |
|----------|---------|--------|
| GCC | 15.2.0 | ✅ Pass |
| Clang | 21.1.8 | ✅ Pass |

#### Ubuntu 24.04 (x86_64, WSL2)

Verified on WSL2 (Linux 6.6.87.2-microsoft-standard-WSL2) with both `Debug`
and `Release` builds. `attest_selftest` reports 74 tests
(73 passed / 1 skipped) and `attest_selftest_c99` reports 12 tests
(11 passed / 1 skipped).

| Compiler | Version | Status |
|----------|---------|--------|
| GCC | 13.3.0 | ✅ Pass |
| Clang | 18.1.3 | ✅ Pass |

#### Windows 11 (arm64)

Verified on a Parallels Desktop VM with Visual Studio Community 2026
(18.2.111415.280) under both `Debug` and `Release` configurations. MSYS2 has
not been tested. `attest_selftest` reports 74 tests (73 passed / 1 skipped)
and `attest_selftest_c99` reports 12 tests (11 passed / 1 skipped).

| Compiler | Version | Threading | Status |
|----------|---------|-----------|--------|
| MSVC `cl` | 19.50.35723.0 (toolset 14.50.35717) | Win32 threads + `__declspec(thread)` | ✅ Pass |
| `clang-cl` | 20.1.8 (`aarch64-pc-windows-msvc`) | C11 `<threads.h>` + `_Thread_local` | ✅ Pass |

Two CP932-locale specific issues were addressed in the course of verification:
MSVC's `/utf-8` is now applied PUBLIC on the `attest` target to silence C4819
on Japanese Windows, and the `Fixture.SetupTeardownCounters` self-test was
rewritten to be independent of test registration order so it survives
`--shuffle`.

#### Windows 11 (x64)

Verified natively on Windows 11 Pro (10.0.26200) with Visual Studio Community
2026 (18.4.11702.344) under both `Debug` and `Release` configurations, built
via the `Ninja Multi-Config` generator. `attest_selftest` reports 74 tests
(73 passed / 1 skipped) and `attest_selftest_c99` reports 12 tests
(11 passed / 1 skipped).

| Compiler | Version | Threading | Status |
|----------|---------|-----------|--------|
| MSVC `cl` | 19.50.35728 (toolset 14.50.35717) | Win32 threads + `__declspec(thread)` | ✅ Pass |
| `clang-cl` | 20.1.8 (`x86_64-pc-windows-msvc`, VS-bundled LLVM) | C11 `<threads.h>` + `_Thread_local` | ✅ Pass |

## Cross-Compiling for Human68k (Sharp X680x0)

attest can be cross-compiled for Human68k using the [elf2x68k](https://github.com/yunkya2/elf2x68k) toolchain:

```bash
cmake -S . -B build-h68k -DCMAKE_TOOLCHAIN_FILE=cmake/human68k.cmake
cmake --build build-h68k
```

The toolchain root is auto-detected in this order:

1. `-DELF2X68K_ROOT=<path>` (CMake option)
2. `ELF2X68K_ROOT` environment variable
3. `brew --prefix elf2x68k` (Homebrew users — zero configuration)
4. Parent directory of `m68k-xelf-gcc` found on `PATH`

If none of these resolve, configuration fails with a message pointing to the install instructions.

## License

MIT No Attribution (MIT-0). See [LICENSE](LICENSE) for details.
