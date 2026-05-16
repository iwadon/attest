# Security Policy

## Supported Versions

attest is currently developed against the `main` branch and has not yet cut
tagged releases. Only the latest commit on `main` is supported with security
fixes. Once tagged releases begin, this policy will be updated to specify
supported version ranges.

## Reporting a Vulnerability

Please report suspected security vulnerabilities **privately** through GitHub's
private vulnerability reporting feature:

1. Visit <https://github.com/iwadon/attest/security/advisories/new>
   (or the "Security" tab of the repository → "Report a vulnerability").
2. Provide a description of the issue, reproduction steps, and the impact you
   believe it has.
3. Where possible, include a minimal proof-of-concept and the affected commit
   SHA.

Do **not** open a public issue, pull request, or discussion for security
reports.

### Response expectations

This is a small, volunteer-maintained project. The maintainer aims to:

- Acknowledge reports within **7 days**.
- Provide an initial assessment within **30 days**.
- Coordinate disclosure timing with the reporter once a fix is ready.

These are best-effort targets, not contractual guarantees.

## Scope

attest is a unit testing framework intended to be linked into other C
projects and executed locally or in CI. The following are considered in scope
for security reports:

- Memory safety issues (out-of-bounds reads/writes, use-after-free, double
  free, uninitialized reads) in the framework's runtime, CLI parser, or
  output formatters.
- Issues that allow attacker-controlled input (test names, filter patterns,
  CLI arguments, captured output) to escalate beyond the test process's own
  privileges.
- Supply chain integrity issues affecting the source distribution itself
  (for example, a tampered upstream tag).

The following are generally **out of scope**:

- Behaviour observed only when the framework is compiled or linked in ways
  the documentation explicitly warns against (for example, calling
  `setjmp`-based helpers across stack frames as noted in `CLAUDE.md`).
- Denial-of-service caused by deliberately pathological filter patterns or
  test inputs supplied by a user who already controls the test process.
- Issues in third-party tools (CMake, compilers, CI runners) that happen to
  surface while building attest.

## Threat model

Because tests run with the privileges of the developer or CI environment that
invokes them, the highest-impact issues are those that let an attacker
compromise attest's source distribution itself (for example, by gaining
maintainer access or by tampering with release artifacts). Reports in that
area are especially welcome.
