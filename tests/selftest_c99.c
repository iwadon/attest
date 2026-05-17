/*
 * C99スモークテスト
 *
 * このテストはC99コンパイラのみで attest フレームワークの基本機能が
 * 動作することを保証するためのものです。`_Generic` を使う `ASSERT_EQ`
 * 系マクロは使わず、明示型マクロ (ASSERT_INT_EQ / ASSERT_UINT_EQ /
 * ASSERT_PTR_EQ など) のみで主要機能をカバーします。
 *
 * 既存の selftest_main.c は C11 (_Generic) を前提としているため、
 * このファイルは attest_selftest と並走する独立した実行ファイルとして
 * ビルド・実行されます。
 */

#include "attest/attest.h"
#include <stddef.h>
#include <string.h>

#if defined(ASSERT_EQ) || defined(EXPECT_EQ) || defined(ASSERT_NE) || defined(EXPECT_NE)    \
	|| defined(ASSERT_LT) || defined(EXPECT_LT) || defined(ASSERT_LE) || defined(EXPECT_LE) \
	|| defined(ASSERT_GT) || defined(EXPECT_GT) || defined(ASSERT_GE) || defined(EXPECT_GE)
#error "C99 builds must not expose C11 _Generic comparison macros"
#endif

/* ------------------------------------------------------------------
 * 明示型マクロの基本動作確認
 * ------------------------------------------------------------------ */

TEST(C99Assert, IntCompare)
{
	ASSERT_INT_EQ(42, 42);
	EXPECT_INT_NE(1, 2);
	EXPECT_INT_LT(3, 5);
	EXPECT_INT_LE(5, 5);
	EXPECT_INT_GT(5, 3);
	EXPECT_INT_GE(5, 5);
}

TEST(C99Assert, UintCompare)
{
	ASSERT_UINT_EQ(0u, 0u);
	EXPECT_UINT_NE(1u, 2u);
	EXPECT_UINT_LT(3u, 5u);
	EXPECT_UINT_GE(7u, 7u);
}

TEST(C99Assert, PointerCompare)
{
	int a = 0;
	int *pa = &a;
	int *pb = pa;
	ASSERT_PTR_EQ(pa, pb);
	EXPECT_PTR_NE(pa, NULL);
}

TEST(C99Assert, NullChecks)
{
	int *p = NULL;
	int x = 0;
	ASSERT_NULL(p);
	EXPECT_NOT_NULL(&x);
}

TEST(C99Assert, Booleans)
{
	ASSERT_TRUE(1);
	EXPECT_FALSE(0);
}

TEST(C99Assert, Strings)
{
	const char *a = "hello";
	const char *b = "hello";
	const char *c = "world";
	ASSERT_STREQ(a, b);
	EXPECT_STRNE(a, c);
}

TEST(C99Assert, Memory)
{
	const unsigned char a[] = { 1, 2, 3, 4 };
	const unsigned char b[] = { 1, 2, 3, 4 };
	const unsigned char c[] = { 1, 2, 3, 5 };
	EXPECT_MEMEQ(a, b, sizeof(a));
	(void)c;
}

TEST(C99Assert, Near)
{
	double a = 1.0;
	double b = 1.0 + 1e-9;
	EXPECT_NEAR(a, b, 1e-6);
}

TEST(C99Assert, CustomMessage)
{
	int x = 7;
	ATT_ASSERT(x == 7, "expected 7 but got %d", x);
	ATT_EXPECT(x > 0, "x should be positive");
}

/* ------------------------------------------------------------------
 * フィクスチャ
 * ------------------------------------------------------------------ */

typedef struct {
	int value;
} C99Fixture;

ATT_FIXTURE_SETUP(C99Fixture)
{
	att_fixture->value = 123;
}

ATT_FIXTURE_TEARDOWN(C99Fixture)
{
	(void)att_fixture;
}

TEST_F(C99Fixture, ProvidesValue)
{
	ASSERT_INT_EQ(att_fixture->value, 123);
}

/* ------------------------------------------------------------------
 * Skip API
 * ------------------------------------------------------------------ */

TEST(C99Skip, ConditionalSkip)
{
	ATT_SKIP_IF(1, "C99 skip smoke test");
	ASSERT_INT_EQ(0, 1); /* この行には到達しないはず */
}

/* ------------------------------------------------------------------
 * SubtestおよびSCOPED_INFO
 * ------------------------------------------------------------------ */

static void c99_subtest_body(void *user)
{
	int *flag = (int *)user;
	*flag = 1;
	EXPECT_INT_EQ(*flag, 1);
}

TEST(C99Subtest, RunsSuccessfully)
{
	int flag = 0;
	att_result result;
	att_status status = att_run_subtest("c99 subtest", c99_subtest_body, &flag, &result);
	ASSERT_INT_EQ((int)status, (int)ATT_STATUS_OK);
	EXPECT_INT_EQ(flag, 1);
	EXPECT_INT_EQ(result.failed, 0);
}

int main(int argc, char **argv)
{
	return attest_main(argc, argv);
}
