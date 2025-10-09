#ifndef ATTEST_ATTEST_H
#define ATTEST_ATTEST_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	ATT_STATUS_OK = 0,
	ATT_STATUS_FAIL = 1,
	ATT_STATUS_ABORTED = 2
} att_status;

typedef struct att_result {
    int total;
    int failed;
    int fatal_failures;
    int nonfatal_failures;
    int skipped;
    att_status status;
} att_result;

typedef void (*att_test_fn)(void);
typedef void (*att_register_fn)(void);

int attest_main(int argc, char** argv);

att_status att_run_subtest(const char* name, void (*fn)(void*), void* user, att_result* out);

typedef struct att_captured {
    const char* data;
    size_t size;
} att_captured;

int att_capture_begin(void);
int att_capture_end(att_captured* out);

void att_register_test(const char* suite, const char* name, att_test_fn fn, const char* file, int line);

#define ATT_COMP_EQ 0
#define ATT_COMP_NE 1
#define ATT_COMP_LT 2
#define ATT_COMP_LE 3
#define ATT_COMP_GT 4
#define ATT_COMP_GE 5

void att_handle_compare_signed(int op, const char* assertion, const char* file, int line, bool fatal,
    const char* lhs_expr, const char* rhs_expr, long long lhs, long long rhs);
void att_handle_compare_unsigned(int op, const char* assertion, const char* file, int line, bool fatal,
    const char* lhs_expr, const char* rhs_expr, unsigned long long lhs, unsigned long long rhs);
void att_handle_compare_double(int op, const char* assertion, const char* file, int line, bool fatal,
    const char* lhs_expr, const char* rhs_expr, double lhs, double rhs);
void att_handle_compare_pointer(int op, const char* assertion, const char* file, int line, bool fatal,
    const char* lhs_expr, const char* rhs_expr, const void* lhs, const void* rhs);
void att_handle_truth(const char* assertion, const char* file, int line, bool fatal, bool value, bool expect_true, const char* expr);
void att_handle_string(int op, const char* assertion, const char* file, int line, bool fatal,
    const char* lhs_expr, const char* rhs_expr, const char* lhs, const char* rhs);
void att_handle_memory(const char* assertion, const char* file, int line, bool fatal,
    const void* lhs, const void* rhs, size_t size, const char* lhs_expr, const char* rhs_expr);
void att_handle_near(const char* assertion, const char* file, int line, bool fatal,
    double lhs, double rhs, double epsilon, const char* lhs_expr, const char* rhs_expr, const char* eps_expr);
void att_handle_subtest_expect(const char* assertion, const char* file, int line, const char* name_expr,
    const char* name_value, int min, int max, att_status status, const att_result* result);
void att_register_manual(const att_register_fn* fns, size_t count);

#define ATT_GENERIC_COMPARE(op, fatal, lhs_value, rhs_value, lhs_expr, rhs_expr, assertion_text) \
	_Generic((lhs_value), \
		char: att_handle_compare_signed, \
		signed char: att_handle_compare_signed, \
		short: att_handle_compare_signed, \
		int: att_handle_compare_signed, \
		long: att_handle_compare_signed, \
		long long: att_handle_compare_signed, \
		unsigned char: att_handle_compare_unsigned, \
		unsigned short: att_handle_compare_unsigned, \
		unsigned int: att_handle_compare_unsigned, \
		unsigned long: att_handle_compare_unsigned, \
		unsigned long long: att_handle_compare_unsigned, \
		float: att_handle_compare_double, \
		double: att_handle_compare_double, \
		long double: att_handle_compare_double, \
		const void*: att_handle_compare_pointer, \
		void*: att_handle_compare_pointer, \
		const char*: att_handle_compare_pointer, \
		char*: att_handle_compare_pointer, \
		default: att_handle_compare_pointer \
	)(op, assertion_text, __FILE__, __LINE__, fatal, lhs_expr, rhs_expr, (lhs_value), (rhs_value))

#define ASSERT_EQ(lhs, rhs) \
	do { \
		ATT_GENERIC_COMPARE(ATT_COMP_EQ, true, lhs, rhs, #lhs, #rhs, "ASSERT_EQ(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_EQ(lhs, rhs) \
	do { \
		ATT_GENERIC_COMPARE(ATT_COMP_EQ, false, lhs, rhs, #lhs, #rhs, "EXPECT_EQ(" #lhs ", " #rhs ")"); \
	} while (0)
#define ASSERT_NE(lhs, rhs) \
	do { \
		ATT_GENERIC_COMPARE(ATT_COMP_NE, true, lhs, rhs, #lhs, #rhs, "ASSERT_NE(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_NE(lhs, rhs) \
	do { \
		ATT_GENERIC_COMPARE(ATT_COMP_NE, false, lhs, rhs, #lhs, #rhs, "EXPECT_NE(" #lhs ", " #rhs ")"); \
	} while (0)
#define ASSERT_LT(lhs, rhs) \
	do { \
		ATT_GENERIC_COMPARE(ATT_COMP_LT, true, lhs, rhs, #lhs, #rhs, "ASSERT_LT(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_LT(lhs, rhs) \
	do { \
		ATT_GENERIC_COMPARE(ATT_COMP_LT, false, lhs, rhs, #lhs, #rhs, "EXPECT_LT(" #lhs ", " #rhs ")"); \
	} while (0)
#define ASSERT_LE(lhs, rhs) \
	do { \
		ATT_GENERIC_COMPARE(ATT_COMP_LE, true, lhs, rhs, #lhs, #rhs, "ASSERT_LE(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_LE(lhs, rhs) \
	do { \
		ATT_GENERIC_COMPARE(ATT_COMP_LE, false, lhs, rhs, #lhs, #rhs, "EXPECT_LE(" #lhs ", " #rhs ")"); \
	} while (0)
#define ASSERT_GT(lhs, rhs) \
	do { \
		ATT_GENERIC_COMPARE(ATT_COMP_GT, true, lhs, rhs, #lhs, #rhs, "ASSERT_GT(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_GT(lhs, rhs) \
	do { \
		ATT_GENERIC_COMPARE(ATT_COMP_GT, false, lhs, rhs, #lhs, #rhs, "EXPECT_GT(" #lhs ", " #rhs ")"); \
	} while (0)
#define ASSERT_GE(lhs, rhs) \
	do { \
		ATT_GENERIC_COMPARE(ATT_COMP_GE, true, lhs, rhs, #lhs, #rhs, "ASSERT_GE(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_GE(lhs, rhs) \
	do { \
		ATT_GENERIC_COMPARE(ATT_COMP_GE, false, lhs, rhs, #lhs, #rhs, "EXPECT_GE(" #lhs ", " #rhs ")"); \
	} while (0)

#define ASSERT_TRUE(expr) \
	do { \
		att_handle_truth("ASSERT_TRUE(" #expr ")", __FILE__, __LINE__, true, !!(expr), true, #expr); \
	} while (0)
#define EXPECT_TRUE(expr) \
	do { \
		att_handle_truth("EXPECT_TRUE(" #expr ")", __FILE__, __LINE__, false, !!(expr), true, #expr); \
	} while (0)
#define ASSERT_FALSE(expr) \
	do { \
		att_handle_truth("ASSERT_FALSE(" #expr ")", __FILE__, __LINE__, true, !!(expr), false, #expr); \
	} while (0)
#define EXPECT_FALSE(expr) \
	do { \
		att_handle_truth("EXPECT_FALSE(" #expr ")", __FILE__, __LINE__, false, !!(expr), false, #expr); \
	} while (0)

#define ASSERT_STREQ(lhs, rhs) \
	do { \
		att_handle_string(ATT_COMP_EQ, "ASSERT_STREQ(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, #lhs, #rhs, (lhs), (rhs)); \
	} while (0)
#define EXPECT_STREQ(lhs, rhs) \
	do { \
		att_handle_string(ATT_COMP_EQ, "EXPECT_STREQ(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, #lhs, #rhs, (lhs), (rhs)); \
	} while (0)
#define ASSERT_STRNE(lhs, rhs) \
	do { \
		att_handle_string(ATT_COMP_NE, "ASSERT_STRNE(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, #lhs, #rhs, (lhs), (rhs)); \
	} while (0)
#define EXPECT_STRNE(lhs, rhs) \
	do { \
		att_handle_string(ATT_COMP_NE, "EXPECT_STRNE(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, #lhs, #rhs, (lhs), (rhs)); \
	} while (0)

#define ASSERT_MEMEQ(lhs, rhs, size) \
	do { \
		att_handle_memory("ASSERT_MEMEQ(" #lhs ", " #rhs ", " #size ")", __FILE__, __LINE__, true, (lhs), (rhs), (size), #lhs, #rhs); \
	} while (0)
#define EXPECT_MEMEQ(lhs, rhs, size) \
	do { \
		att_handle_memory("EXPECT_MEMEQ(" #lhs ", " #rhs ", " #size ")", __FILE__, __LINE__, false, (lhs), (rhs), (size), #lhs, #rhs); \
	} while (0)

#define ASSERT_NEAR(lhs, rhs, eps) \
	do { \
		att_handle_near("ASSERT_NEAR(" #lhs ", " #rhs ", " #eps ")", __FILE__, __LINE__, true, (double)(lhs), (double)(rhs), (double)(eps), #lhs, #rhs, #eps); \
	} while (0)
#define EXPECT_NEAR(lhs, rhs, eps) \
	do { \
		att_handle_near("EXPECT_NEAR(" #lhs ", " #rhs ", " #eps ")", __FILE__, __LINE__, false, (double)(lhs), (double)(rhs), (double)(eps), #lhs, #rhs, #eps); \
	} while (0)

#define ATT_EXPECT_SUBTEST_FAILS(name, block, min, max) \
	do { \
		att_result att__sub_result; \
		att_status att__sub_status = att_run_subtest((name), (block), NULL, &att__sub_result); \
		att_handle_subtest_expect("ATT_EXPECT_SUBTEST_FAILS(" #name ", " #min ", " #max ")", \
		    __FILE__, __LINE__, #name, (name), (min), (max), att__sub_status, &att__sub_result); \
	} while (0)

#define ATT_CONCAT_INNER(a, b) a##b
#define ATT_CONCAT(a, b) ATT_CONCAT_INNER(a, b)
#define ATT_CONCAT3(a, b, c) ATT_CONCAT(ATT_CONCAT(a, b), c)
#define ATT_SUFFIX(name) _##name

#if defined(__COUNTER__)
#define ATT_UNIQUE_ID(prefix) ATT_CONCAT(prefix, __COUNTER__)
#else
#define ATT_UNIQUE_ID(prefix) ATT_CONCAT(prefix, __LINE__)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define ATT_AUTOREG(fn) \
	static void fn(void) __attribute__((constructor)); \
	static void fn(void)
#elif defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
#define ATT_AUTOREG(fn) \
	static void __cdecl fn(void); \
	__declspec(allocate(".CRT$XCU")) void(__cdecl* fn##_ptr)(void) = fn; \
	static void __cdecl fn(void)
#else
#define ATT_AUTOREG(fn) static void fn(void)
#endif

#define ATT_TEST_FN_NAME(Suite, Name) ATT_CONCAT3(att_test_fn_, Suite, ATT_SUFFIX(Name))
#define ATT_REGISTER_FN_NAME(Suite, Name) ATT_CONCAT3(att_register_, Suite, ATT_SUFFIX(Name))

#define ATT_TEST_IMPL(suite, name, fn_symbol, reg_symbol)         \
	static void fn_symbol(void);                                  \
	ATT_AUTOREG(reg_symbol)                                       \
	{                                                             \
		att_register_test(suite, name, fn_symbol, __FILE__, __LINE__); \
	}                                                             \
	static void fn_symbol(void)

#define ATT_TEST(Suite, Name) \
	ATT_TEST_IMPL(#Suite, #Name, ATT_TEST_FN_NAME(Suite, Name), ATT_REGISTER_FN_NAME(Suite, Name))

#define ATT_TEST_REF(Suite, Name) ATT_REGISTER_FN_NAME(Suite, Name)

#define ATT_REGISTER_TESTS(...) \
	do { \
		const att_register_fn att__manual_fns[] = { __VA_ARGS__ }; \
		att_register_manual(att__manual_fns, sizeof(att__manual_fns) / sizeof(att__manual_fns[0])); \
	} while (0)

#define TEST(Suite, Name) ATT_TEST(Suite, Name)

#ifdef __cplusplus
}
#endif

#endif /* ATTEST_ATTEST_H */
