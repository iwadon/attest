#include <stdio.h>
#include "attest/attest.h"

TEST(QuickTimeout, ShouldTimeout)
{
	/* Perform many assertions - should timeout after 50ms */
	for (int i = 0; i < 100000000; ++i) {
		EXPECT_TRUE(true);
	}
	/* Should never reach here */
	ASSERT_TRUE(false);
}

int main(int argc, char **argv)
{
	return attest_main(argc, argv);
}
