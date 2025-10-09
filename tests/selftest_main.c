#include <stdio.h>
#include <string.h>

#include "attest/attest.h"

static void att_subtest_nonfatal(void* user)
{
	(void)user;
	EXPECT_TRUE(0);
}

static void att_subtest_fatal(void* user)
{
	(void)user;
	ASSERT_TRUE(0);
}

static void att_macro_mismatch(void* user)
{
	(void)user;
	ATT_EXPECT_SUBTEST_FAILS("nonfatal", att_subtest_nonfatal, 0, 0);
}

static void att_formatting_eq_failure(void* user)
{
	(void)user;
	int expected = 42;
	int actual = 24;
	EXPECT_EQ(expected, actual);
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
	const char* lhs = "attest";
	const char* rhs = "attest";
	ASSERT_STREQ(lhs, rhs);
	EXPECT_STRNE(lhs, "other");
}

TEST(Assert, Memory)
{
	unsigned char lhs[] = {1, 2, 3, 4};
	unsigned char rhs[] = {1, 2, 3, 4};
	ASSERT_MEMEQ(lhs, rhs, sizeof(lhs));
	EXPECT_MEMEQ(NULL, NULL, 0);
}

TEST(Assert, Near)
{
	EXPECT_NEAR(3.14159, 3.1416, 0.0001);
}

TEST(Subtest, ReportsFailures)
{
	att_result result;
	att_status status = att_run_subtest("nonfatal", att_subtest_nonfatal, NULL, &result);
	ASSERT_EQ(ATT_STATUS_FAIL, status);
	ASSERT_EQ(1, result.failed);
	EXPECT_EQ(1, result.nonfatal_failures);
}

TEST(Subtest, RecordsAbort)
{
	att_result result;
	att_status status = att_run_subtest("fatal", att_subtest_fatal, NULL, &result);
	ASSERT_EQ(ATT_STATUS_ABORTED, status);
	EXPECT_EQ(1, result.fatal_failures);
}

TEST(Subtest, ExpectFailsMacroPasses)
{
	ATT_EXPECT_SUBTEST_FAILS("nonfatal", att_subtest_nonfatal, 1, 1);
}

TEST(Subtest, ExpectFailsMacroRegistersFailure)
{
	att_result result;
	att_status status = att_run_subtest("macro mismatch", att_macro_mismatch, NULL, &result);
	ASSERT_EQ(ATT_STATUS_FAIL, status);
	EXPECT_EQ(1, result.nonfatal_failures);
}

TEST(Output, EqualityFailureFormatting)
{
	ASSERT_EQ(0, att_capture_begin());
	att_result result;
	att_status status = att_run_subtest("formatting", att_formatting_eq_failure, NULL, &result);
	att_captured captured;
	ASSERT_EQ(0, att_capture_end(&captured));
	ASSERT_EQ(ATT_STATUS_FAIL, status);
	ASSERT_TRUE(captured.data != NULL);

	const char* expected_line = strstr(captured.data, "    expected: 42");
	ASSERT_TRUE(expected_line != NULL);
	const char* actual_line = strstr(captured.data, "      actual: 24");
	ASSERT_TRUE(actual_line != NULL);
	EXPECT_TRUE(expected_line < actual_line);

	const char* expr_line = strstr(captured.data, "    expr: expected=42, actual=24");
	ASSERT_TRUE(expr_line != NULL);
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
}

int main(int argc, char** argv)
{
	return attest_main(argc, argv);
}
