# attest P1.1 Specification

This document defines the feature extensions for the P1.1 stage of attest. It assumes the behavior of the P1 implementation and specifies new features to be added.

---

## 1. Floating-Point Comparison Enhancements

### 1.1 Relative Error and ULP Comparison

**Status: Not Implemented**

To provide more robust floating-point comparisons, the following macros will be introduced:

- `*_NEAR_REL(a, b, rel_eps)`: Asserts that two floating-point numbers are close to each other within a relative error margin.
- `*_ULP_EQ(a, b, max_ulp)`: Asserts that two floating-point numbers are equal within a specified ULP (Units in the Last Place) distance.

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
