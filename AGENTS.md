# Repository Guidelines

## Project Structure & Module Organization
The repository currently ships the canonical specification under `docs/attest_p0_spec.md`, `docs/attest_p1_spec.md`, and `docs/attest_p1_plus_spec.md`. Place public headers in `include/attest/` (for example `include/attest/assert.h`) and implementations in `src/`, mirrored by module (`src/assert.c`, `src/runner/cli.c`, etc.). Shared helpers that are not part of the public API belong in `src/internal/`. Self-tests and examples live under `tests/`; prefer suite folders such as `tests/selftest/` and `tests/integration/`, keeping fixtures beside the suite that consumes them. Keep build artifacts in an out-of-tree `build/` directory so the working tree stays clean.

## Build, Test, and Development Commands
- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` configures an out-of-source build.
- `cmake --build build` compiles the library and the attest self-test binary.
- `ctest --test-dir build --output-on-failure` runs the bundled suites; mirror CLI filters via `ATT_TEST_ARGS="--filter=Suite.*" ctest ...`.
- `cmake --build build --target format` runs `clang-format` on tracked sources when the target is defined.
- `cmake --build build --target coverage` (optional) should emit `build/coverage/index.html`; gate sizable changes on meaningful coverage once the target exists.

## Coding Style & Naming Conventions
Formatting is enforced by `.clang-format` (WebKit base, tabs width 4, pointer alignment right) and `.editorconfig` (LF endings, tabs for C/C++). Run `clang-format -i path/to/file.c` before submitting patches. Module filenames use snake_case (`assert_engine.c`), public symbols carry the `att_` prefix with lowercase underscores (`att_summary`), and macros stay upper snake (`ATT_EXPECT_EQ`). Test suite identifiers must follow `Suite.Name` per the P0 spec. Keep functions within the 100-column guideline and wrap multi-line strings as required by the formatting config.

## Testing Guidelines
Follow the P0 execution model: default to `ASSERT_*` for fatal checks and `EXPECT_*` when the suite should continue. Group cases with `TEST(Suite, Name)` macros and keep fixture setup in helpers until `TEST_F` support lands (see the P1 roadmap). Add regression coverage in `tests/selftest/` that reproduces defects before fixing them. Run the full matrix with `ctest` and, for CLI smoke, execute `build/attest --list` and `build/attest --filter=<pattern>` manually. Document new CLI switches in `docs/` along with representative output.

## Commit & Pull Request Guidelines
History follows Conventional Commits (`feat:`, `fix:`, `docs:`). Keep subject lines under 72 characters and use the body for rationale, spec paragraph references, and risks. Each pull request should describe the change, link the tracking issue, enumerate the test commands you ran, and attach screenshots or logs when altering CLI output. Request a second reviewer whenever behaviour or public headers shift.

## Reference Docs & Roadmap
Review `docs/attest_p0_spec.md` before changing behaviour and call out the affected sections in commits. For P1 features consult `docs/attest_p1_spec.md`; longer-term items remain in `docs/attest_p1_plus_spec.md`. Tag TODOs with the planned stage (`// TODO(P1):`) and keep new configuration flags disabled by default unless ratified by the spec. Surface any security-sensitive changes (allocators, signal handlers) in the PR checklist.
