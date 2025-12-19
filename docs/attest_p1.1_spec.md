# attest P1.1 Specification

This document defines the feature extensions for the P1.1 stage of attest. It assumes the behavior of the P1 implementation and specifies new features to be added.

---

## 1. Floating-Point Comparison Enhancements

### 1.1 Relative Error and ULP Comparison

**Status: Implemented**

To provide more robust floating-point comparisons, the following macros are available:

- `ASSERT_NEAR_REL(a, b, rel_eps)` / `EXPECT_NEAR_REL(a, b, rel_eps)`: Asserts that two floating-point numbers are close to each other within a relative error margin.
- `ASSERT_ULP_EQ(a, b, max_ulp)` / `EXPECT_ULP_EQ(a, b, max_ulp)`: Asserts that two floating-point numbers are equal within a specified ULP (Units in the Last Place) distance.

#### NEAR_REL Usage

```c
// Relative error comparison: |a - b| <= rel_eps * max(|a|, |b|)
EXPECT_NEAR_REL(100.0, 101.0, 0.02);  // passes (1% difference, 2% tolerance)
EXPECT_NEAR_REL(1e10, 1e10 + 1e8, 0.02);  // passes (large values)
ASSERT_NEAR_REL(-1000.0, -1005.0, 0.01);  // passes (negative values)
```

Special cases:
- If both values are near zero (< 1e-15), uses absolute comparison with rel_eps as threshold
- NaN inputs always fail
- ±Infinity values must match exactly (same sign)
- Negative rel_eps fails the assertion

#### ULP_EQ Usage

```c
// ULP (Units in Last Place) comparison
EXPECT_ULP_EQ(1.0, 1.0, 0);  // exact match
EXPECT_ULP_EQ(1.0, nextafter(1.0, 2.0), 1);  // 1 ULP apart
ASSERT_ULP_EQ(0.0, -0.0, 0);  // +0 and -0 are 0 ULPs apart
```

Special cases:
- Handles sign transitions correctly (e.g., smallest positive vs smallest negative)
- Works with denormal numbers
- NaN inputs always fail
- ±Infinity must match exactly
- Negative max_ulp fails the assertion

---

## 2. Scoped Contextual Information

### 2.1 SCOPED_INFO

**Status: Implemented**

A macro to add contextual information to a scope. This information will be printed in case of a test failure within that scope, helping to diagnose issues in data-driven tests or loops.

#### Usage

```c
SCOPED_INFO("iteration=%d", i);
EXPECT_EQ(expected, actual);
```

When a failure occurs, the output will include the context stack:

```
[  FAILED  ] Suite.Name
  context: iteration=5
  /path/to/test.c:42: EXPECT_EQ(expected, actual) failed.
    expected: 10
      actual: 5
```

#### Implementation Details

- Uses GCC/Clang `__attribute__((cleanup))` for automatic scope management
- Context information is pushed onto a stack when `SCOPED_INFO` is invoked
- Automatically pops from the stack when the scope is exited (via normal exit, early return, or exception)
- Multiple `SCOPED_INFO` calls can be nested (stack depth limit: 8)
- Context is only printed when an assertion fails within the scope

#### Example

```c
TEST(Math, LoopTest)
{
    for (int i = 0; i < 10; i++) {
        SCOPED_INFO("iteration=%d", i);
        int result = compute(i);
        EXPECT_GT(result, 0);  // If this fails, shows which iteration failed
    }
}
```
