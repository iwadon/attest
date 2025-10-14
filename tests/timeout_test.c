#include <stdio.h>
#include <stdlib.h>
#include "attest/attest.h"

/*
 * Manual timeout verification tests
 *
 * These tests are designed to be run manually with --timeout-ms flag
 * to verify that the timeout mechanism works correctly. They are NOT
 * part of the automated test suite because:
 *
 * 1. They require explicit --timeout-ms flag to succeed
 * 2. Without timeout, they would run forever or for a very long time
 * 3. Their expected behavior is to FAIL due to timeout
 *
 * Usage examples:
 *   attest_timeout_test --timeout-ms=50   # Should timeout quickly
 *   attest_timeout_test --timeout-ms=100  # Should timeout in assertions
 */

TEST(QuickTimeout, ShouldTimeout)
{
	/* Perform many assertions - should timeout after 50ms */
	for (int i = 0; i < 100000000; ++i) {
		EXPECT_TRUE(true);
	}
	/* Should never reach here */
	ASSERT_TRUE(false);
}

TEST(Timeout, InfiniteLoop)
{
	/*
	 * This test verifies timeout detection in a pure infinite loop
	 * without any assertions.
	 *
	 * Run with: attest_timeout_test --timeout-ms=50 --filter=Timeout.InfiniteLoop
	 * Expected: Should timeout and report failure
	 */
	const char *env = getenv("ATTEST_ENABLE_TIMEOUT_TEST");
	if (!env) {
		ATT_SKIP("Set ATTEST_ENABLE_TIMEOUT_TEST=1 to enable");
	}
	for (;;) {
		/* Infinite loop - timeout mechanism should kill this */
	}
}

TEST(Timeout, WithAsserts)
{
	/*
	 * This test verifies timeout detection during assertion-heavy loops.
	 * On Windows, this exercises the periodic timeout check in assertion macros.
	 *
	 * Run with: attest_timeout_test --timeout-ms=100 --filter=Timeout.WithAsserts
	 * Expected: Should timeout and report failure during the assertion loop
	 */
	const char *env = getenv("ATTEST_ENABLE_TIMEOUT_WITH_ASSERTS_TEST");
	if (!env) {
		ATT_SKIP("Set ATTEST_ENABLE_TIMEOUT_WITH_ASSERTS_TEST=1 to enable");
	}
	/* This test should timeout during the infinite loop with assertions */
	for (int i = 0; ; ++i) {
		EXPECT_TRUE(true);  /* Keep asserting so Windows can check timeout */
	}
}

int main(int argc, char **argv)
{
	return attest_main(argc, argv);
}
