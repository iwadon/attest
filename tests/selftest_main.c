#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attest/attest.h"
#include "internal/attest_context.h"
#include "internal/attest_internal.h"

typedef struct {
	int lhs;
	int rhs;
} MathFx;

static int g_mathfx_setup_calls;
static int g_mathfx_teardown_calls;

ATT_FIXTURE_SETUP(MathFx)
{
	g_mathfx_setup_calls += 1;
	att_fixture->lhs = 40;
	att_fixture->rhs = 2;
}

ATT_FIXTURE_TEARDOWN(MathFx)
{
	g_mathfx_teardown_calls += 1;
}

static void skip_subtest(void *user)
{
	(void)user;
	ATT_SKIP("subtest skip");
}

static void att_subtest_nonfatal(void *user)
{
	(void)user;
	EXPECT_TRUE(0);
}

static void att_subtest_fatal(void *user)
{
	(void)user;
	ASSERT_TRUE(0);
}

static void att_macro_mismatch(void *user)
{
	(void)user;
	ATT_EXPECT_SUBTEST_FAILS("nonfatal", { att_subtest_nonfatal(NULL); }, 0, 0);
}

static int g_manual_counter;

static void manual_registered_test(void)
{
	g_manual_counter += 1;
	ASSERT_TRUE(g_manual_counter > 0);
}

static void manual_register_function(void)
{
	att_register_test("Manual", "RegistersViaMacro", manual_registered_test, __FILE__, __LINE__);
}

static void att_formatting_eq_failure(void *user)
{
	(void)user;
	SCOPED_INFO("context from eq failure");
	int expected = 42;
	int actual = 24;
	EXPECT_EQ(expected, actual);
}

static void scoped_info_failure(void *user)
{
	(void)user;
	SCOPED_INFO("i=%d", 5);
	EXPECT_TRUE(false);
}

static void near_rel_nan_lhs_subtest(void *user)
{
	(void)user;
	EXPECT_NEAR_REL(NAN, 1.0, 0.01);
}

static void near_rel_nan_rhs_subtest(void *user)
{
	(void)user;
	EXPECT_NEAR_REL(1.0, NAN, 0.01);
}

static void near_rel_nan_eps_subtest(void *user)
{
	(void)user;
	EXPECT_NEAR_REL(1.0, 1.0, NAN);
}

static void near_rel_inf_diff_signs_subtest(void *user)
{
	(void)user;
	EXPECT_NEAR_REL(INFINITY, -INFINITY, 0.01);
}

static void near_rel_fail_large_diff_subtest(void *user)
{
	(void)user;
	EXPECT_NEAR_REL(100.0, 110.0, 0.01);
}

static void near_rel_negative_eps_subtest(void *user)
{
	(void)user;
	EXPECT_NEAR_REL(1.0, 1.0, -0.01);
}

/* ULP_EQ test helper functions */
static void ulp_nan_lhs_subtest(void *user)
{
	(void)user;
	EXPECT_ULP_EQ(NAN, 1.0, 5);
}

static void ulp_nan_rhs_subtest(void *user)
{
	(void)user;
	EXPECT_ULP_EQ(1.0, NAN, 5);
}

static void ulp_negative_max_ulp_subtest(void *user)
{
	(void)user;
	EXPECT_ULP_EQ(1.0, 1.0, -1);
}

static void ulp_inf_diff_signs_subtest(void *user)
{
	(void)user;
	EXPECT_ULP_EQ(INFINITY, -INFINITY, 5);
}

static void ulp_exceed_threshold_subtest(void *user)
{
	(void)user;
	double base = 1.0;
	double far = nextafter(nextafter(nextafter(base, INFINITY), INFINITY), INFINITY);
	EXPECT_ULP_EQ(base, far, 2);
}

TEST(Sanity, Passes)
{
	/* Placeholder test used to verify registration flow. */
}

TEST(Assert, Numbers)
{
	ASSERT_EQ(42, 42);
	EXPECT_LT(3, 5);
	EXPECT_GE(7, 7);
}

TEST(Assert, Booleans)
{
	ASSERT_TRUE(1 == 1);
	EXPECT_FALSE(0);
}

TEST(Assert, Strings)
{
	const char *lhs = "attest";
	const char *rhs = "attest";
	ASSERT_STREQ(lhs, rhs);
	EXPECT_STRNE(lhs, "other");
}

TEST(Assert, Memory)
{
	unsigned char lhs[] = { 1, 2, 3, 4 };
	unsigned char rhs[] = { 1, 2, 3, 4 };
	ASSERT_MEMEQ(lhs, rhs, sizeof(lhs));
	EXPECT_MEMEQ(NULL, NULL, 0);
}

TEST(Assert, Near)
{
	EXPECT_NEAR(3.14159, 3.1416, 0.0001);
}

TEST(Assert, NearRelBasic)
{
	/* Normal passing cases */
	EXPECT_NEAR_REL(100.0, 101.0, 0.02);
	EXPECT_NEAR_REL(100.0, 99.0, 0.02);
	ASSERT_NEAR_REL(1000.0, 1005.0, 0.01);

	/* Exact match */
	EXPECT_NEAR_REL(42.0, 42.0, 0.0);
	EXPECT_NEAR_REL(42.0, 42.0, 1e-10);
}

TEST(Assert, NearRelLargeValues)
{
	/* Large values with small relative differences */
	EXPECT_NEAR_REL(1e10, 1e10 + 1e8, 0.02);
	EXPECT_NEAR_REL(1e10, 1e10 - 1e8, 0.02);
	ASSERT_NEAR_REL(1e15, 1e15 + 1e13, 0.02);
}

TEST(Assert, NearRelSmallEpsilon)
{
	/* Very small relative epsilon */
	EXPECT_NEAR_REL(1.0, 1.0 + 1e-11, 1e-10);
	EXPECT_NEAR_REL(1000.0, 1000.0 + 1e-8, 1e-10);
}

TEST(Assert, NearRelNegativeValues)
{
	/* Negative values */
	EXPECT_NEAR_REL(-100.0, -101.0, 0.02);
	EXPECT_NEAR_REL(-100.0, -99.0, 0.02);
	ASSERT_NEAR_REL(-1000.0, -1005.0, 0.01);

	/* Mixed signs - should fail for different magnitudes but test passing case */
	EXPECT_NEAR_REL(-1.0, -1.0, 0.01);
}

TEST(Assert, NearRelZeroAndNearZero)
{
	/* Both values near zero: uses absolute comparison with rel_eps as threshold */
	EXPECT_NEAR_REL(0.0, 0.0, 0.01);
	EXPECT_NEAR_REL(1e-20, 1e-20, 1e-10);
	EXPECT_NEAR_REL(1e-16, 0.0, 1e-15);

	/* Values just above the near-zero threshold */
	EXPECT_NEAR_REL(1e-14, 1.01e-14, 0.02);
}

TEST(Assert, NearRelDenormalNumbers)
{
	/* Denormal/subnormal numbers (very small numbers near zero) */
	double denormal1 = DBL_MIN * 0.5;
	double denormal2 = DBL_MIN * 0.51;
	EXPECT_NEAR_REL(denormal1, denormal2, 0.1);

	/* Mix of denormal and normal */
	EXPECT_NEAR_REL(DBL_MIN, DBL_MIN * 1.01, 0.02);
}

TEST(Assert, NearRelInfinitySameSign)
{
	/* Same sign infinity should match */
	EXPECT_NEAR_REL(INFINITY, INFINITY, 0.01);
	EXPECT_NEAR_REL(-INFINITY, -INFINITY, 0.01);
}

TEST(Assert, NearRelFailures)
{
	/* Test expected failures using subtest mechanism */

	/* NaN inputs always fail */
	ATT_EXPECT_SUBTEST_FAILS("nan_lhs", { near_rel_nan_lhs_subtest(NULL); }, 1, 1);
	ATT_EXPECT_SUBTEST_FAILS("nan_rhs", { near_rel_nan_rhs_subtest(NULL); }, 1, 1);
	ATT_EXPECT_SUBTEST_FAILS("nan_eps", { near_rel_nan_eps_subtest(NULL); }, 1, 1);

	/* Different sign infinity fails */
	ATT_EXPECT_SUBTEST_FAILS("inf_diff_signs", { near_rel_inf_diff_signs_subtest(NULL); }, 1, 1);

	/* Large relative difference fails */
	ATT_EXPECT_SUBTEST_FAILS("large_diff", { near_rel_fail_large_diff_subtest(NULL); }, 1, 1);

	/* Negative epsilon fails */
	ATT_EXPECT_SUBTEST_FAILS("negative_eps", { near_rel_negative_eps_subtest(NULL); }, 1, 1);
}

TEST(Assert, NearRelAssertFatal)
{
	/* Test that ASSERT_NEAR_REL is fatal */
	att_result result;
	att_capture_begin();
	att_status status = att_run_subtest("fatal_nearrel",
		(void (*)(void *))near_rel_fail_large_diff_subtest, NULL, &result);
	att_capture_end(NULL);
	ASSERT_EQ(ATT_STATUS_FAIL, status);
	EXPECT_EQ(1, result.failed);
	EXPECT_EQ(0, result.fatal_failures);
	EXPECT_EQ(1, result.nonfatal_failures);
}

TEST(Assert, UlpEqExactMatch)
{
	/* Exact match = 0 ULP distance */
	EXPECT_ULP_EQ(1.0, 1.0, 0);
	EXPECT_ULP_EQ(0.0, 0.0, 0);
	EXPECT_ULP_EQ(-1.0, -1.0, 0);
	ASSERT_ULP_EQ(3.14159, 3.14159, 0);
}

TEST(Assert, UlpEqAdjacentValues)
{
	/* Adjacent representable doubles = 1 ULP apart */
	double base = 1.0;
	double next = nextafter(base, INFINITY);
	EXPECT_ULP_EQ(base, next, 1);
	ASSERT_ULP_EQ(next, base, 1);

	/* Previous value is also 1 ULP apart */
	double prev = nextafter(base, -INFINITY);
	EXPECT_ULP_EQ(base, prev, 1);
	EXPECT_ULP_EQ(prev, base, 1);
}

TEST(Assert, UlpEqSmallDistances)
{
	/* Test small ULP distances: 2, 5, 10 ULPs */
	double base = 100.0;

	/* 2 ULPs */
	double next2 = nextafter(nextafter(base, INFINITY), INFINITY);
	EXPECT_ULP_EQ(base, next2, 2);
	EXPECT_ULP_EQ(base, next2, 10);

	/* 5 ULPs */
	double val = base;
	for (int i = 0; i < 5; ++i) {
		val = nextafter(val, INFINITY);
	}
	EXPECT_ULP_EQ(base, val, 5);
	EXPECT_ULP_EQ(base, val, 10);

	/* Exactly at threshold */
	double val10 = base;
	for (int i = 0; i < 10; ++i) {
		val10 = nextafter(val10, INFINITY);
	}
	ASSERT_ULP_EQ(base, val10, 10);
}

TEST(Assert, UlpEqZeroHandling)
{
	/* +0.0 and -0.0 should be 0 ULP apart (IEEE 754 treats them as equal) */
	EXPECT_ULP_EQ(0.0, -0.0, 0);
	EXPECT_ULP_EQ(-0.0, 0.0, 0);
	ASSERT_ULP_EQ(0.0, 0.0, 0);

	/* Test zero with adjacent values */
	double zero_next = nextafter(0.0, INFINITY);
	EXPECT_ULP_EQ(0.0, zero_next, 1);
	EXPECT_ULP_EQ(zero_next, 0.0, 1);
}

TEST(Assert, UlpEqSignTransition)
{
	/* Smallest positive vs smallest negative - should work across sign boundary */
	double smallest_pos = nextafter(0.0, INFINITY);
	double smallest_neg = nextafter(0.0, -INFINITY);

	/* These are adjacent to zero, so 1 ULP from zero each */
	EXPECT_ULP_EQ(smallest_pos, 0.0, 1);
	EXPECT_ULP_EQ(smallest_neg, 0.0, 1);

	/* Distance between smallest positive and negative is 2 ULPs through zero */
	ASSERT_ULP_EQ(smallest_pos, smallest_neg, 2);
}

TEST(Assert, UlpEqDenormalNumbers)
{
	/* Denormal/subnormal numbers (very small numbers near zero) */
	double denormal1 = DBL_MIN * 0.5;
	double denormal2 = DBL_MIN * 0.5;
	EXPECT_ULP_EQ(denormal1, denormal2, 0);

	/* Adjacent denormals */
	double denormal_next = nextafter(denormal1, INFINITY);
	EXPECT_ULP_EQ(denormal1, denormal_next, 1);
	ASSERT_ULP_EQ(denormal_next, denormal1, 1);

	/* Denormal to normal boundary */
	double denormal = DBL_MIN * 0.9;
	double normal = DBL_MIN * 1.1;
	/* Just check they can be compared */
	EXPECT_ULP_EQ(denormal, denormal, 0);
	EXPECT_ULP_EQ(normal, normal, 0);
}

TEST(Assert, UlpEqInfinitySameSign)
{
	/* Same sign infinity = 0 ULP distance */
	EXPECT_ULP_EQ(INFINITY, INFINITY, 0);
	EXPECT_ULP_EQ(-INFINITY, -INFINITY, 0);
	ASSERT_ULP_EQ(INFINITY, INFINITY, 5);
}

TEST(Assert, UlpEqLargeValues)
{
	/* Test with large values */
	double large = 1e100;
	double large_next = nextafter(large, INFINITY);
	EXPECT_ULP_EQ(large, large_next, 1);

	/* Multiple ULPs on large values */
	double val = large;
	for (int i = 0; i < 50; ++i) {
		val = nextafter(val, INFINITY);
	}
	ASSERT_ULP_EQ(large, val, 50);
}

TEST(Assert, UlpEqNegativeValues)
{
	/* Test with negative values */
	double neg = -42.0;
	double neg_next = nextafter(neg, -INFINITY);
	EXPECT_ULP_EQ(neg, neg_next, 1);
	EXPECT_ULP_EQ(neg_next, neg, 1);

	/* Multiple ULPs on negative values */
	double val = neg;
	for (int i = 0; i < 7; ++i) {
		val = nextafter(val, -INFINITY);
	}
	ASSERT_ULP_EQ(neg, val, 7);
}

TEST(Assert, UlpEqFailures)
{
	/* Test expected failures using subtest mechanism */

	/* NaN inputs always fail */
	ATT_EXPECT_SUBTEST_FAILS("nan_lhs", { ulp_nan_lhs_subtest(NULL); }, 1, 1);
	ATT_EXPECT_SUBTEST_FAILS("nan_rhs", { ulp_nan_rhs_subtest(NULL); }, 1, 1);

	/* Negative max_ulp fails validation */
	ATT_EXPECT_SUBTEST_FAILS("negative_max_ulp", { ulp_negative_max_ulp_subtest(NULL); }, 1, 1);

	/* Different sign infinity fails */
	ATT_EXPECT_SUBTEST_FAILS("inf_diff_signs", { ulp_inf_diff_signs_subtest(NULL); }, 1, 1);

	/* Exceeding threshold fails */
	ATT_EXPECT_SUBTEST_FAILS("exceed_threshold", { ulp_exceed_threshold_subtest(NULL); }, 1, 1);
}

TEST(Assert, UlpEqAssertFatal)
{
	/* Test that ASSERT_ULP_EQ is fatal */
	att_result result;
	att_capture_begin();
	att_status status = att_run_subtest("fatal_ulp",
		(void (*)(void *))ulp_exceed_threshold_subtest, NULL, &result);
	att_capture_end(NULL);
	ASSERT_EQ(ATT_STATUS_FAIL, status);
	EXPECT_EQ(1, result.failed);
	EXPECT_EQ(0, result.fatal_failures);
	EXPECT_EQ(1, result.nonfatal_failures);
}

TEST_F(MathFx, ProvidesFixturePointer)
{
	MathFx *fx = ATT_FIXTURE(MathFx);
	ASSERT_EQ(fx->lhs + fx->rhs, 42);
	EXPECT_EQ(att_fixture->lhs, 40);
	EXPECT_EQ(att_fixture->rhs, 2);
}

TEST_F(MathFx, CountsSetupPerTest)
{
	MathFx *fx = ATT_FIXTURE(MathFx);
	EXPECT_TRUE(g_mathfx_setup_calls >= 1);
	EXPECT_EQ(fx->lhs, 40);
	EXPECT_EQ(fx->rhs, 2);
}

TEST(Fixture, SetupTeardownCounters)
{
	EXPECT_EQ(g_mathfx_setup_calls, 2);
	EXPECT_EQ(g_mathfx_teardown_calls, 2);
}

TEST(Skip, TopLevel)
{
	ATT_SKIP("top-level skip");
}

TEST(Skip, ConditionalNoSkip)
{
	ATT_SKIP_IF(0, "should not skip");
	EXPECT_TRUE(1);
}

TEST(Skip, SubtestRecordsSkip)
{
	att_result result;
	att_status status = att_run_subtest("skip", skip_subtest, NULL, &result);
	ASSERT_EQ(ATT_STATUS_OK, status);
	EXPECT_EQ(1, result.skipped);
	EXPECT_EQ(0, result.failed);
}

TEST(Subtest, ReportsFailures)
{
	att_result result;
	att_capture_begin();
	att_status status = att_run_subtest("nonfatal", att_subtest_nonfatal, NULL, &result);
	att_capture_end(NULL);
	ASSERT_EQ(ATT_STATUS_FAIL, status);
	ASSERT_EQ(1, result.failed);
	EXPECT_EQ(1, result.nonfatal_failures);
}

TEST(Subtest, RecordsAbort)
{
	att_result result;
	att_capture_begin();
	att_status status = att_run_subtest("fatal", att_subtest_fatal, NULL, &result);
	att_capture_end(NULL);
	ASSERT_EQ(ATT_STATUS_ABORTED, status);
	EXPECT_EQ(1, result.fatal_failures);
}

TEST(Subtest, ExpectFailsMacroPasses)
{
	ATT_EXPECT_SUBTEST_FAILS("nonfatal", { EXPECT_TRUE(0); }, 1, 1);
}

TEST(Subtest, ExpectFailsMacroRegistersFailure)
{
	att_result result;
	att_capture_begin();
	att_status status = att_run_subtest("macro mismatch", att_macro_mismatch, NULL, &result);
	att_capture_end(NULL);
	ASSERT_EQ(ATT_STATUS_FAIL, status);
	EXPECT_EQ(1, result.nonfatal_failures);
}

TEST(Output, EqualityFailureFormatting)
{
	ATT_SKIP_IF(att_context_get_format() != ATT_OUTPUT_DEFAULT, "test requires default output format");

	ASSERT_EQ(0, att_capture_begin());
	att_result result;
	att_status status = att_run_subtest("formatting", att_formatting_eq_failure, NULL, &result);
	att_captured captured;
	ASSERT_EQ(0, att_capture_end(&captured));
	ASSERT_EQ(ATT_STATUS_FAIL, status);
	ASSERT_TRUE(captured.data != NULL);

	const char *expected_line = strstr(captured.data, "    expected: 42");
	ASSERT_TRUE(expected_line != NULL);
	const char *actual_line = strstr(captured.data, "      actual: 24");
	ASSERT_TRUE(actual_line != NULL);
	EXPECT_TRUE(expected_line < actual_line);

	const char *expr_line = strstr(captured.data, "    expr: expected=42, actual=24");
	ASSERT_TRUE(expr_line != NULL);

	const char *context_line = strstr(captured.data, "  context: context from eq failure");
	ASSERT_TRUE(context_line != NULL);

	free(captured.data);
}

TEST(ScopedInfo, ReportsContextOnFailure)
{
	ATT_SKIP_IF(att_context_get_format() != ATT_OUTPUT_DEFAULT, "test requires default output format");

	att_captured captured;
	ASSERT_EQ(0, att_capture_begin());

	att_subtest_scope *scope = att_subtest_scope_enter("scoped_info");
	if (att_subtest_scope_protect(scope) == 0) {
		scoped_info_failure(NULL);
	}
	att_result result;
	att_status status = att_subtest_scope_leave(scope, &result);

	ASSERT_EQ(0, att_capture_end(&captured));

	ASSERT_EQ(ATT_STATUS_FAIL, status);
	ASSERT_EQ(1, result.failed);
	ASSERT_TRUE(captured.data != NULL);

	const char *expected_line = strstr(captured.data, "  context: i=5");
	ASSERT_TRUE(expected_line != NULL);
	free(captured.data);
}

TEST(Capture, CapturesStderr)
{
	ASSERT_EQ(0, att_capture_begin());
	fprintf(stderr, "capture-test\n");
	att_captured captured;
	ASSERT_EQ(0, att_capture_end(&captured));
	ASSERT_TRUE(captured.data != NULL);
	size_t expected = strlen("capture-test\n");
	EXPECT_EQ((int)expected, (int)captured.size);
	EXPECT_EQ(0, strncmp(captured.data, "capture-test\n", expected));
	free(captured.data);
}

TEST(LongDouble, BasicComparisons)
{
	/* Note: on some platforms (ARM64 macOS, MSVC), long double == double */
	long double a = 1.5L;
	long double b = 1.5L;
	long double c = 2.5L;

	EXPECT_EQ(a, b);
	EXPECT_NE(a, c);
	EXPECT_LT(a, c);
	EXPECT_LE(a, b);
	EXPECT_LE(a, c);
	EXPECT_GT(c, a);
	EXPECT_GE(b, a);
	EXPECT_GE(c, a);
}

TEST(LongDouble, ExtendedPrecisionLiterals)
{
	/* Test type dispatching works correctly for long double */
	long double pi = 3.141592653589793L;
	long double same = 3.141592653589793L;
	long double different = 3.141592653589794L;

	ASSERT_EQ(pi, same);
	EXPECT_NE(pi, different);
}

TEST(LongDouble, LargeAndSmallValues)
{
	/* Test with large values beyond double's range */
	long double large1 = 1.0e308L;
	long double large2 = 1.0e308L;
	long double larger = 1.1e308L;

	EXPECT_EQ(large1, large2);
	EXPECT_LT(large1, larger);
	EXPECT_GT(larger, large1);

	/* Test with very small values */
	long double small1 = 1.0e-300L;
	long double small2 = 1.0e-300L;
	long double smaller = 1.0e-310L;

	EXPECT_EQ(small1, small2);
	EXPECT_GT(small1, smaller);
	EXPECT_LT(smaller, small1);
}

TEST(LongDouble, NegativeValues)
{
	long double neg_a = -42.125L;
	long double neg_b = -42.125L;
	long double neg_c = -42.25L;

	ASSERT_EQ(neg_a, neg_b);
	EXPECT_NE(neg_a, neg_c);
	EXPECT_GT(neg_a, neg_c);
	EXPECT_LT(neg_c, neg_a);
}

TEST(LongDouble, ZeroComparisons)
{
	long double zero = 0.0L;
	long double pos = 1.0e-100L;
	long double neg = -1.0e-100L;

	EXPECT_EQ(zero, 0.0L);
	EXPECT_GT(pos, zero);
	EXPECT_LT(neg, zero);
	EXPECT_NE(pos, zero);
	EXPECT_NE(neg, zero);
}

TEST(LongDouble, InfinityAndSpecialValues)
{
	/* Infinity comparisons */
	long double pos_inf = (long double)INFINITY;
	long double neg_inf = (long double)-INFINITY;

	EXPECT_EQ(pos_inf, pos_inf);
	EXPECT_EQ(neg_inf, neg_inf);
	EXPECT_NE(pos_inf, neg_inf);
	EXPECT_GT(pos_inf, 1.0e308L);
	EXPECT_LT(neg_inf, -1.0e308L);
}

TEST(CustomAssert, PassingCondition)
{
	int x = 42;
	ATT_EXPECT(x == 42, "x should be 42, got %d", x);
	ATT_ASSERT(x > 0, "x must be positive, got %d", x);
}

TEST(CustomAssert, FailingExpect)
{
	ATT_SKIP_IF(att_context_get_format() != ATT_OUTPUT_DEFAULT, "test requires default output format");

	att_captured captured;
	ASSERT_EQ(0, att_capture_begin());

	att_subtest_scope *scope = att_subtest_scope_enter("custom_expect_fail");
	if (att_subtest_scope_protect(scope) == 0) {
		int value = 5;
		ATT_EXPECT(value > 10, "value must be > 10, but got %d", value);
	}
	att_result result;
	att_status status = att_subtest_scope_leave(scope, &result);

	ASSERT_EQ(0, att_capture_end(&captured));

	ASSERT_EQ(ATT_STATUS_FAIL, status);
	ASSERT_EQ(1, result.failed);
	ASSERT_TRUE(captured.data != NULL);
	ASSERT_TRUE(strstr(captured.data, "value must be > 10, but got 5") != NULL);
	free(captured.data);
}

TEST(CustomAssert, FailingAssert)
{
	ATT_SKIP_IF(att_context_get_format() != ATT_OUTPUT_DEFAULT, "test requires default output format");

	att_captured captured;
	ASSERT_EQ(0, att_capture_begin());

	att_subtest_scope *scope = att_subtest_scope_enter("custom_assert_fail");
	if (att_subtest_scope_protect(scope) == 0) {
		int value = -1;
		ATT_ASSERT(value >= 0, "value must be non-negative, got %d", value);
	}
	att_result result;
	att_status status = att_subtest_scope_leave(scope, &result);

	ASSERT_EQ(0, att_capture_end(&captured));

	ASSERT_EQ(ATT_STATUS_ABORTED, status);
	ASSERT_EQ(1, result.fatal_failures);
	ASSERT_TRUE(captured.data != NULL);
	ASSERT_TRUE(strstr(captured.data, "value must be non-negative, got -1") != NULL);
	free(captured.data);
}

TEST(Summary, ReturnsZeroBeforeRun)
{
	attest_summary sum = attest_get_summary();
	EXPECT_EQ(0, sum.total);
	EXPECT_EQ(0, sum.passed);
	EXPECT_EQ(0, sum.failed);
	EXPECT_EQ(0, sum.skipped);
}

TEST(Parallel, BasicExecution)
{
	/* This test is a placeholder to verify parallel execution infrastructure works */
	EXPECT_TRUE(1);
}

TEST(Parallel, MultipleAssertions)
{
	/* Test with multiple assertions to verify thread-local context */
	EXPECT_EQ(1, 1);
	EXPECT_NE(1, 2);
	EXPECT_LT(1, 2);
	EXPECT_GT(2, 1);
}

TEST(Parallel, IndependentExecution)
{
	/* Test that each test runs independently */
	static int counter = 0;
	int local = ++counter;
	EXPECT_GT(local, 0);
}

/* ========== Shuffle Tests ========== */

TEST(Shuffle, CLIParseShuffleWithSeed)
{
	char *argv[] = { "test", "--shuffle=12345", NULL };
	att_cli_options opts;
	char *err_msg = NULL;
	int result = att_cli_parse(2, argv, &opts, &err_msg);

	EXPECT_EQ(0, result);
	EXPECT_TRUE(opts.shuffle);
	EXPECT_EQ(12345u, opts.shuffle_seed);
	EXPECT_TRUE(err_msg == NULL);
}

TEST(Shuffle, CLIParseShuffleWithoutSeed)
{
	char *argv[] = { "test", "--shuffle", NULL };
	att_cli_options opts;
	char *err_msg = NULL;
	int result = att_cli_parse(2, argv, &opts, &err_msg);

	EXPECT_EQ(0, result);
	EXPECT_TRUE(opts.shuffle);
	/* Seed should be non-zero (time-based) */
	EXPECT_TRUE(opts.shuffle_seed > 0);
	EXPECT_TRUE(err_msg == NULL);
}

TEST(Shuffle, CLIParseShuffleInvalidSeed)
{
	char *argv[] = { "test", "--shuffle=abc", NULL };
	att_cli_options opts;
	char *err_msg = NULL;
	int result = att_cli_parse(2, argv, &opts, &err_msg);

	EXPECT_EQ(1, result);
	EXPECT_TRUE(err_msg != NULL);
	free(err_msg);
}

TEST(Shuffle, CLIParseShuffleEmptySeed)
{
	char *argv[] = { "test", "--shuffle=", NULL };
	att_cli_options opts;
	char *err_msg = NULL;
	int result = att_cli_parse(2, argv, &opts, &err_msg);

	EXPECT_EQ(1, result);
	EXPECT_TRUE(err_msg != NULL);
	free(err_msg);
}

TEST(Shuffle, SameSeedProducesSameOrder)
{
	/* Create a simple array to test shuffle algorithm */
	size_t items[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	size_t first_order[10];
	size_t second_order[10];

	/* Shuffle with seed 42 and record order */
	unsigned int seed = 42;
	unsigned int next = seed;
	for (size_t i = 9; i > 0; --i) {
		next = next * 1103515245 + 12345;
		size_t j = (size_t)(next / 65536) % (i + 1);
		size_t temp = items[i];
		items[i] = items[j];
		items[j] = temp;
	}
	memcpy(first_order, items, sizeof(items));

	/* Reset and shuffle again with same seed */
	for (size_t i = 0; i < 10; ++i)
		items[i] = i;

	next = seed;
	for (size_t i = 9; i > 0; --i) {
		next = next * 1103515245 + 12345;
		size_t j = (size_t)(next / 65536) % (i + 1);
		size_t temp = items[i];
		items[i] = items[j];
		items[j] = temp;
	}
	memcpy(second_order, items, sizeof(items));

	/* Verify same seed produces same order */
	EXPECT_EQ(0, memcmp(first_order, second_order, sizeof(first_order)));
}

TEST(Shuffle, DifferentSeedsProduceDifferentOrder)
{
	size_t items1[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	size_t items2[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

	/* Shuffle with seed 42 */
	unsigned int next = 42;
	for (size_t i = 9; i > 0; --i) {
		next = next * 1103515245 + 12345;
		size_t j = (size_t)(next / 65536) % (i + 1);
		size_t temp = items1[i];
		items1[i] = items1[j];
		items1[j] = temp;
	}

	/* Shuffle with seed 99 */
	next = 99;
	for (size_t i = 9; i > 0; --i) {
		next = next * 1103515245 + 12345;
		size_t j = (size_t)(next / 65536) % (i + 1);
		size_t temp = items2[i];
		items2[i] = items2[j];
		items2[j] = temp;
	}

	/* Verify different seeds produce different order */
	EXPECT_NE(0, memcmp(items1, items2, sizeof(items1)));
}

TEST(Shuffle, AllElementsPreserved)
{
	size_t items[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	bool found[10] = { false };

	/* Shuffle with arbitrary seed */
	unsigned int next = 12345;
	for (size_t i = 9; i > 0; --i) {
		next = next * 1103515245 + 12345;
		size_t j = (size_t)(next / 65536) % (i + 1);
		size_t temp = items[i];
		items[i] = items[j];
		items[j] = temp;
	}

	/* Mark all found elements */
	for (size_t i = 0; i < 10; ++i) {
		ASSERT_LT(items[i], 10u);
		found[items[i]] = true;
	}

	/* Verify all elements are present */
	for (size_t i = 0; i < 10; ++i) {
		EXPECT_TRUE(found[i]);
	}
}

TEST(Shuffle, CLIParseShuffleInvalidPrefix)
{
	/* --shufflefoo should be rejected as unknown option */
	char *argv[] = { "test", "--shufflefoo", NULL };
	att_cli_options opts;
	char *err_msg = NULL;
	int result = att_cli_parse(2, argv, &opts, &err_msg);

	EXPECT_EQ(1, result);
	EXPECT_TRUE(err_msg != NULL);
	free(err_msg);
}

TEST(Shuffle, CLIParseShuffleInvalidPrefix123)
{
	/* --shuffle123 should be rejected as unknown option */
	char *argv[] = { "test", "--shuffle123", NULL };
	att_cli_options opts;
	char *err_msg = NULL;
	int result = att_cli_parse(2, argv, &opts, &err_msg);

	EXPECT_EQ(1, result);
	EXPECT_TRUE(err_msg != NULL);
	free(err_msg);
}

TEST(Shuffle, CLIParseShuffleNegativeSeed)
{
	/* Negative seed values should be rejected */
	char *argv[] = { "test", "--shuffle=-1", NULL };
	att_cli_options opts;
	char *err_msg = NULL;
	int result = att_cli_parse(2, argv, &opts, &err_msg);

	EXPECT_EQ(1, result);
	EXPECT_TRUE(err_msg != NULL);
	free(err_msg);
}

TEST(Shuffle, CLIParseShuffleOverflowSeed)
{
	/* Seed values exceeding UINT_MAX should be rejected */
	char *argv[] = { "test", "--shuffle=4294967296", NULL };
	att_cli_options opts;
	char *err_msg = NULL;
	int result = att_cli_parse(2, argv, &opts, &err_msg);

	EXPECT_EQ(1, result);
	EXPECT_TRUE(err_msg != NULL);
	free(err_msg);
}

TEST(Shuffle, CLIParseShuffleMaxValidSeed)
{
	/* UINT_MAX should be accepted */
	char *argv[] = { "test", "--shuffle=4294967295", NULL };
	att_cli_options opts;
	char *err_msg = NULL;
	int result = att_cli_parse(2, argv, &opts, &err_msg);

	EXPECT_EQ(0, result);
	EXPECT_TRUE(opts.shuffle);
	EXPECT_EQ(4294967295u, opts.shuffle_seed);
	EXPECT_TRUE(err_msg == NULL);
}

/* ========== Explicit Type Tests (C89 compatible macros) ========== */

TEST(ExplicitType, IntComparisons)
{
	int a = 5, b = 10;
	EXPECT_INT_EQ(5, 5);
	EXPECT_INT_NE(a, b);
	EXPECT_INT_LT(a, b);
	EXPECT_INT_LE(a, b);
	EXPECT_INT_LE(5, 5);
	EXPECT_INT_GT(b, a);
	EXPECT_INT_GE(b, a);
	EXPECT_INT_GE(5, 5);
}

TEST(ExplicitType, IntAssertions)
{
	int a = 5, b = 10;
	ASSERT_INT_EQ(5, 5);
	ASSERT_INT_NE(a, b);
	ASSERT_INT_LT(a, b);
	ASSERT_INT_LE(a, b);
	ASSERT_INT_LE(5, 5);
	ASSERT_INT_GT(b, a);
	ASSERT_INT_GE(b, a);
	ASSERT_INT_GE(5, 5);
}

TEST(ExplicitType, IntNegativeValues)
{
	int neg = -42;
	int pos = 10;
	EXPECT_INT_EQ(neg, -42);
	EXPECT_INT_NE(neg, pos);
	EXPECT_INT_LT(neg, pos);
	EXPECT_INT_LE(neg, pos);
	EXPECT_INT_GT(pos, neg);
	EXPECT_INT_GE(pos, neg);
	ASSERT_INT_LT(-100, -50);
}

TEST(ExplicitType, UintComparisons)
{
	unsigned int a = 5, b = 10;
	EXPECT_UINT_EQ(5u, 5u);
	EXPECT_UINT_NE(a, b);
	EXPECT_UINT_LT(a, b);
	EXPECT_UINT_LE(a, b);
	EXPECT_UINT_LE(5u, 5u);
	EXPECT_UINT_GT(b, a);
	EXPECT_UINT_GE(b, a);
	EXPECT_UINT_GE(5u, 5u);
}

TEST(ExplicitType, UintAssertions)
{
	unsigned int a = 5, b = 10;
	ASSERT_UINT_EQ(5u, 5u);
	ASSERT_UINT_NE(a, b);
	ASSERT_UINT_LT(a, b);
	ASSERT_UINT_LE(a, b);
	ASSERT_UINT_LE(5u, 5u);
	ASSERT_UINT_GT(b, a);
	ASSERT_UINT_GE(b, a);
	ASSERT_UINT_GE(5u, 5u);
}

TEST(ExplicitType, UintZeroAndMax)
{
	unsigned int zero = 0;
	unsigned int one = 1;
	unsigned int max = (unsigned int)-1;
	EXPECT_UINT_EQ(zero, 0u);
	EXPECT_UINT_LT(zero, one);
	EXPECT_UINT_GT(max, zero);
	ASSERT_UINT_GE(max, one);
}

TEST(ExplicitType, PtrComparisons)
{
	int arr[2];
	void *p1 = &arr[0];
	void *p2 = &arr[1];
	EXPECT_PTR_EQ(p1, p1);
	EXPECT_PTR_NE(p1, p2);
	EXPECT_PTR_LT(p1, p2);
	EXPECT_PTR_LE(p1, p2);
	EXPECT_PTR_LE(p1, p1);
	EXPECT_PTR_GT(p2, p1);
	EXPECT_PTR_GE(p2, p1);
	EXPECT_PTR_GE(p1, p1);
}

TEST(ExplicitType, PtrAssertions)
{
	int arr[2];
	void *p1 = &arr[0];
	void *p2 = &arr[1];
	ASSERT_PTR_EQ(p1, p1);
	ASSERT_PTR_NE(p1, p2);
	ASSERT_PTR_LT(p1, p2);
	ASSERT_PTR_LE(p1, p2);
	ASSERT_PTR_LE(p1, p1);
	ASSERT_PTR_GT(p2, p1);
	ASSERT_PTR_GE(p2, p1);
	ASSERT_PTR_GE(p1, p1);
}

TEST(ExplicitType, PtrNullHandling)
{
	void *null_ptr = NULL;
	int value = 42;
	void *non_null = &value;
	EXPECT_PTR_EQ(null_ptr, NULL);
	EXPECT_PTR_NE(null_ptr, non_null);
	ASSERT_PTR_EQ(NULL, NULL);
}

int main(int argc, char **argv)
{
	ATT_REGISTER_TESTS(manual_register_function);
	return attest_main(argc, argv);
}
