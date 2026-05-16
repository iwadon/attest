#ifndef ATTEST_ATTEST_H
#define ATTEST_ATTEST_H

/* Platform detection for setjmp/longjmp */
/* Human68k (Sharp X680x0 series): m68k-based platform with POSIX-like APIs */
#if defined(__human68k__) || defined(__human68k)
#define ATT_PLATFORM_HUMAN68K
/* Unix-like systems: Linux, macOS, BSD, etc. */
#elif defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#define ATT_PLATFORM_POSIX
#endif

/* Enable POSIX features for sigjmp_buf/sigsetjmp/siglongjmp */
#ifdef ATT_PLATFORM_POSIX
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

/* Platform-specific includes for setjmp/longjmp */
#ifdef ATT_PLATFORM_POSIX
#include <signal.h> /* For sigjmp_buf, sigsetjmp, siglongjmp */
#endif
#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* setjmp/longjmp abstraction */
#ifdef ATT_PLATFORM_POSIX
/* POSIX: Use signal-aware setjmp to preserve signal mask */
typedef sigjmp_buf att_jmp_buf;
#define att_setjmp(env) sigsetjmp((env), 1)
#define att_longjmp(env, val) siglongjmp((env), (val))
#else
/* Human68k, Windows, and others: Use standard C setjmp/longjmp */
typedef jmp_buf att_jmp_buf;
#define att_setjmp(env) setjmp(env)
#define att_longjmp(env, val) longjmp((env), (val))
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

typedef struct attest_summary {
	int total;
	int passed;
	int failed;
	int skipped;
} attest_summary;

typedef void (*att_test_fn)(void);
typedef void (*att_register_fn)(void);

typedef void (*att_fixture_hook)(void *);
typedef void (*att_fixture_body_fn)(void *);

int attest_main(int argc, char **argv);
attest_summary attest_get_summary(void);

att_status att_run_subtest(const char *name, void (*fn)(void *), void *user, att_result *out);

/* Opaque subtest scope structure */
typedef struct att_subtest_scope att_subtest_scope;
att_subtest_scope *att_subtest_scope_enter(const char *name);

/* Internal helper - do not call directly */
int att__subtest_scope_active(const att_subtest_scope *scope);
att_jmp_buf *att__get_abort_env_ptr(void);

/* Must be a macro to avoid stack frame issues with setjmp/longjmp.
 * setjmp must be called in the same stack frame that will handle longjmp.
 * This macro expands setjmp inline in the caller's stack frame. */
#define att_subtest_scope_protect(scope) \
	(att__subtest_scope_active(scope) ? att_setjmp(*att__get_abort_env_ptr()) : 1)

att_status att_subtest_scope_leave(att_subtest_scope *scope, att_result *out);

typedef struct att_captured {
	/* Caller owns `data` and must free it with `free` when done. */
	char *data;
	size_t size;
} att_captured;

int att_capture_begin(void);
int att_capture_end(att_captured *out);

void att_register_test(const char *suite, const char *name, att_test_fn fn, const char *file, int line);

#define ATT_COMP_EQ 0
#define ATT_COMP_NE 1
#define ATT_COMP_LT 2
#define ATT_COMP_LE 3
#define ATT_COMP_GT 4
#define ATT_COMP_GE 5

void att_handle_compare_signed(int op, const char *assertion, const char *file, int line, bool fatal,
	const char *lhs_expr, const char *rhs_expr, long long lhs, long long rhs);
void att_handle_compare_unsigned(int op, const char *assertion, const char *file, int line, bool fatal,
	const char *lhs_expr, const char *rhs_expr, unsigned long long lhs, unsigned long long rhs);
void att_handle_compare_double(int op, const char *assertion, const char *file, int line, bool fatal,
	const char *lhs_expr, const char *rhs_expr, double lhs, double rhs);
void att_handle_compare_long_double(int op, const char *assertion, const char *file, int line, bool fatal,
	const char *lhs_expr, const char *rhs_expr, long double lhs, long double rhs);
void att_handle_compare_pointer(int op, const char *assertion, const char *file, int line, bool fatal,
	const char *lhs_expr, const char *rhs_expr, const void *lhs, const void *rhs);
void att_handle_truth(const char *assertion, const char *file, int line, bool fatal, bool value, bool expect_true, const char *expr);
void att_handle_custom_assert(const char *file, int line, bool fatal, bool value, const char *expr, const char *fmt, ...);
void att_handle_string(int op, const char *assertion, const char *file, int line, bool fatal,
	const char *lhs_expr, const char *rhs_expr, const char *lhs, const char *rhs);
void att_handle_memory(const char *assertion, const char *file, int line, bool fatal,
	const void *lhs, const void *rhs, size_t size, const char *lhs_expr, const char *rhs_expr);
void att_handle_near(const char *assertion, const char *file, int line, bool fatal,
	double lhs, double rhs, double epsilon, const char *lhs_expr, const char *rhs_expr, const char *eps_expr);
void att_handle_near_rel(const char *assertion, const char *file, int line, bool fatal,
	double lhs, double rhs, double rel_eps, const char *lhs_expr, const char *rhs_expr, const char *eps_expr);
void att_handle_ulp_eq(double a, double b, int64_t max_ulp, const char *file, int line,
	const char *expr_a, const char *expr_b, const char *expr_ulp, bool fatal);
bool att_handle_subtest_expect(const char *assertion, const char *file, int line, const char *name_expr,
	const char *name_value, int min, int max, att_status status, const att_result *result);
void att_replay_captured(const att_captured *captured);
void att_register_manual(const att_register_fn *fns, size_t count);
void att_fixture_register_setup(const char *fixture_name, size_t fixture_size, att_fixture_hook fn);
void att_fixture_register_teardown(const char *fixture_name, size_t fixture_size, att_fixture_hook fn);
void att_fixture_run(const char *fixture_name, size_t fixture_size, att_fixture_body_fn body_fn);
void att_skip(const char *reason);

typedef struct att_info_scope att_info_scope_t;

void att_info_scope_push(const char *fmt, ...);
void att_info_scope_pop_impl(void *unused);

struct att_info_scope {
	int dummy;
};

/* C11 _Generic-based type dispatch - only available in C11 or later */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L

#define ATT_GENERIC_COMPARE(op, fatal, lhs_value, rhs_value, lhs_expr, rhs_expr, assertion_text) \
	_Generic((lhs_value),                                                                        \
		char: att_handle_compare_signed,                                                         \
		signed char: att_handle_compare_signed,                                                  \
		short: att_handle_compare_signed,                                                        \
		int: att_handle_compare_signed,                                                          \
		long: att_handle_compare_signed,                                                         \
		long long: att_handle_compare_signed,                                                    \
		unsigned char: att_handle_compare_unsigned,                                              \
		unsigned short: att_handle_compare_unsigned,                                             \
		unsigned int: att_handle_compare_unsigned,                                               \
		unsigned long: att_handle_compare_unsigned,                                              \
		unsigned long long: att_handle_compare_unsigned,                                         \
		float: att_handle_compare_double,                                                        \
		double: att_handle_compare_double,                                                       \
		long double: att_handle_compare_long_double,                                             \
		const void *: att_handle_compare_pointer,                                                \
		void *: att_handle_compare_pointer,                                                      \
		const char *: att_handle_compare_pointer,                                                \
		char *: att_handle_compare_pointer,                                                      \
		default: att_handle_compare_signed)(op, assertion_text, __FILE__, __LINE__, fatal, lhs_expr, rhs_expr, (lhs_value), (rhs_value))

#define ASSERT_EQ(lhs, rhs)                                                                            \
	do {                                                                                               \
		ATT_GENERIC_COMPARE(ATT_COMP_EQ, true, lhs, rhs, #lhs, #rhs, "ASSERT_EQ(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_EQ(lhs, rhs)                                                                             \
	do {                                                                                                \
		ATT_GENERIC_COMPARE(ATT_COMP_EQ, false, lhs, rhs, #lhs, #rhs, "EXPECT_EQ(" #lhs ", " #rhs ")"); \
	} while (0)
#define ASSERT_NE(lhs, rhs)                                                                            \
	do {                                                                                               \
		ATT_GENERIC_COMPARE(ATT_COMP_NE, true, lhs, rhs, #lhs, #rhs, "ASSERT_NE(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_NE(lhs, rhs)                                                                             \
	do {                                                                                                \
		ATT_GENERIC_COMPARE(ATT_COMP_NE, false, lhs, rhs, #lhs, #rhs, "EXPECT_NE(" #lhs ", " #rhs ")"); \
	} while (0)
#define ASSERT_LT(lhs, rhs)                                                                            \
	do {                                                                                               \
		ATT_GENERIC_COMPARE(ATT_COMP_LT, true, lhs, rhs, #lhs, #rhs, "ASSERT_LT(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_LT(lhs, rhs)                                                                             \
	do {                                                                                                \
		ATT_GENERIC_COMPARE(ATT_COMP_LT, false, lhs, rhs, #lhs, #rhs, "EXPECT_LT(" #lhs ", " #rhs ")"); \
	} while (0)
#define ASSERT_LE(lhs, rhs)                                                                            \
	do {                                                                                               \
		ATT_GENERIC_COMPARE(ATT_COMP_LE, true, lhs, rhs, #lhs, #rhs, "ASSERT_LE(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_LE(lhs, rhs)                                                                             \
	do {                                                                                                \
		ATT_GENERIC_COMPARE(ATT_COMP_LE, false, lhs, rhs, #lhs, #rhs, "EXPECT_LE(" #lhs ", " #rhs ")"); \
	} while (0)
#define ASSERT_GT(lhs, rhs)                                                                            \
	do {                                                                                               \
		ATT_GENERIC_COMPARE(ATT_COMP_GT, true, lhs, rhs, #lhs, #rhs, "ASSERT_GT(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_GT(lhs, rhs)                                                                             \
	do {                                                                                                \
		ATT_GENERIC_COMPARE(ATT_COMP_GT, false, lhs, rhs, #lhs, #rhs, "EXPECT_GT(" #lhs ", " #rhs ")"); \
	} while (0)
#define ASSERT_GE(lhs, rhs)                                                                            \
	do {                                                                                               \
		ATT_GENERIC_COMPARE(ATT_COMP_GE, true, lhs, rhs, #lhs, #rhs, "ASSERT_GE(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_GE(lhs, rhs)                                                                             \
	do {                                                                                                \
		ATT_GENERIC_COMPARE(ATT_COMP_GE, false, lhs, rhs, #lhs, #rhs, "EXPECT_GE(" #lhs ", " #rhs ")"); \
	} while (0)

#else
/* Pre-C11 / no _Generic support: fall back to signed-integer comparison.
 * Callers needing float, pointer, or unsigned semantics should use the
 * explicit per-type macros (ASSERT_INT_EQ, ASSERT_DBL_NEAR, ASSERT_STR_EQ, etc.). */

#define ATT_FALLBACK_COMPARE(op, fatal, lhs_value, rhs_value, lhs_expr, rhs_expr, assertion_text) \
	att_handle_compare_signed((op), (assertion_text), __FILE__, __LINE__, (fatal),                \
		(lhs_expr), (rhs_expr), (long long)(lhs_value), (long long)(rhs_value))

#define ASSERT_EQ(lhs, rhs)                                                                             \
	do {                                                                                                \
		ATT_FALLBACK_COMPARE(ATT_COMP_EQ, true, lhs, rhs, #lhs, #rhs, "ASSERT_EQ(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_EQ(lhs, rhs)                                                                              \
	do {                                                                                                 \
		ATT_FALLBACK_COMPARE(ATT_COMP_EQ, false, lhs, rhs, #lhs, #rhs, "EXPECT_EQ(" #lhs ", " #rhs ")"); \
	} while (0)
#define ASSERT_NE(lhs, rhs)                                                                             \
	do {                                                                                                \
		ATT_FALLBACK_COMPARE(ATT_COMP_NE, true, lhs, rhs, #lhs, #rhs, "ASSERT_NE(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_NE(lhs, rhs)                                                                              \
	do {                                                                                                 \
		ATT_FALLBACK_COMPARE(ATT_COMP_NE, false, lhs, rhs, #lhs, #rhs, "EXPECT_NE(" #lhs ", " #rhs ")"); \
	} while (0)
#define ASSERT_LT(lhs, rhs)                                                                             \
	do {                                                                                                \
		ATT_FALLBACK_COMPARE(ATT_COMP_LT, true, lhs, rhs, #lhs, #rhs, "ASSERT_LT(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_LT(lhs, rhs)                                                                              \
	do {                                                                                                 \
		ATT_FALLBACK_COMPARE(ATT_COMP_LT, false, lhs, rhs, #lhs, #rhs, "EXPECT_LT(" #lhs ", " #rhs ")"); \
	} while (0)
#define ASSERT_LE(lhs, rhs)                                                                             \
	do {                                                                                                \
		ATT_FALLBACK_COMPARE(ATT_COMP_LE, true, lhs, rhs, #lhs, #rhs, "ASSERT_LE(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_LE(lhs, rhs)                                                                              \
	do {                                                                                                 \
		ATT_FALLBACK_COMPARE(ATT_COMP_LE, false, lhs, rhs, #lhs, #rhs, "EXPECT_LE(" #lhs ", " #rhs ")"); \
	} while (0)
#define ASSERT_GT(lhs, rhs)                                                                             \
	do {                                                                                                \
		ATT_FALLBACK_COMPARE(ATT_COMP_GT, true, lhs, rhs, #lhs, #rhs, "ASSERT_GT(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_GT(lhs, rhs)                                                                              \
	do {                                                                                                 \
		ATT_FALLBACK_COMPARE(ATT_COMP_GT, false, lhs, rhs, #lhs, #rhs, "EXPECT_GT(" #lhs ", " #rhs ")"); \
	} while (0)
#define ASSERT_GE(lhs, rhs)                                                                             \
	do {                                                                                                \
		ATT_FALLBACK_COMPARE(ATT_COMP_GE, true, lhs, rhs, #lhs, #rhs, "ASSERT_GE(" #lhs ", " #rhs ")"); \
	} while (0)
#define EXPECT_GE(lhs, rhs)                                                                              \
	do {                                                                                                 \
		ATT_FALLBACK_COMPARE(ATT_COMP_GE, false, lhs, rhs, #lhs, #rhs, "EXPECT_GE(" #lhs ", " #rhs ")"); \
	} while (0)

#endif /* C11 _Generic support */

/* C89-compatible explicit type macros - INT (signed) */
#define ASSERT_INT_EQ(lhs, rhs)                                                                               \
	do {                                                                                                      \
		att_handle_compare_signed(ATT_COMP_EQ, "ASSERT_INT_EQ(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (long long)(lhs), (long long)(rhs));                                                  \
	} while (0)
#define EXPECT_INT_EQ(lhs, rhs)                                                                                \
	do {                                                                                                       \
		att_handle_compare_signed(ATT_COMP_EQ, "EXPECT_INT_EQ(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (long long)(lhs), (long long)(rhs));                                                   \
	} while (0)
#define ASSERT_INT_NE(lhs, rhs)                                                                               \
	do {                                                                                                      \
		att_handle_compare_signed(ATT_COMP_NE, "ASSERT_INT_NE(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (long long)(lhs), (long long)(rhs));                                                  \
	} while (0)
#define EXPECT_INT_NE(lhs, rhs)                                                                                \
	do {                                                                                                       \
		att_handle_compare_signed(ATT_COMP_NE, "EXPECT_INT_NE(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (long long)(lhs), (long long)(rhs));                                                   \
	} while (0)
#define ASSERT_INT_LT(lhs, rhs)                                                                               \
	do {                                                                                                      \
		att_handle_compare_signed(ATT_COMP_LT, "ASSERT_INT_LT(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (long long)(lhs), (long long)(rhs));                                                  \
	} while (0)
#define EXPECT_INT_LT(lhs, rhs)                                                                                \
	do {                                                                                                       \
		att_handle_compare_signed(ATT_COMP_LT, "EXPECT_INT_LT(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (long long)(lhs), (long long)(rhs));                                                   \
	} while (0)
#define ASSERT_INT_LE(lhs, rhs)                                                                               \
	do {                                                                                                      \
		att_handle_compare_signed(ATT_COMP_LE, "ASSERT_INT_LE(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (long long)(lhs), (long long)(rhs));                                                  \
	} while (0)
#define EXPECT_INT_LE(lhs, rhs)                                                                                \
	do {                                                                                                       \
		att_handle_compare_signed(ATT_COMP_LE, "EXPECT_INT_LE(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (long long)(lhs), (long long)(rhs));                                                   \
	} while (0)
#define ASSERT_INT_GT(lhs, rhs)                                                                               \
	do {                                                                                                      \
		att_handle_compare_signed(ATT_COMP_GT, "ASSERT_INT_GT(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (long long)(lhs), (long long)(rhs));                                                  \
	} while (0)
#define EXPECT_INT_GT(lhs, rhs)                                                                                \
	do {                                                                                                       \
		att_handle_compare_signed(ATT_COMP_GT, "EXPECT_INT_GT(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (long long)(lhs), (long long)(rhs));                                                   \
	} while (0)
#define ASSERT_INT_GE(lhs, rhs)                                                                               \
	do {                                                                                                      \
		att_handle_compare_signed(ATT_COMP_GE, "ASSERT_INT_GE(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (long long)(lhs), (long long)(rhs));                                                  \
	} while (0)
#define EXPECT_INT_GE(lhs, rhs)                                                                                \
	do {                                                                                                       \
		att_handle_compare_signed(ATT_COMP_GE, "EXPECT_INT_GE(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (long long)(lhs), (long long)(rhs));                                                   \
	} while (0)

/* C89-compatible explicit type macros - UINT (unsigned) */
#define ASSERT_UINT_EQ(lhs, rhs)                                                                                 \
	do {                                                                                                         \
		att_handle_compare_unsigned(ATT_COMP_EQ, "ASSERT_UINT_EQ(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (unsigned long long)(lhs), (unsigned long long)(rhs));                                   \
	} while (0)
#define EXPECT_UINT_EQ(lhs, rhs)                                                                                  \
	do {                                                                                                          \
		att_handle_compare_unsigned(ATT_COMP_EQ, "EXPECT_UINT_EQ(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (unsigned long long)(lhs), (unsigned long long)(rhs));                                    \
	} while (0)
#define ASSERT_UINT_NE(lhs, rhs)                                                                                 \
	do {                                                                                                         \
		att_handle_compare_unsigned(ATT_COMP_NE, "ASSERT_UINT_NE(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (unsigned long long)(lhs), (unsigned long long)(rhs));                                   \
	} while (0)
#define EXPECT_UINT_NE(lhs, rhs)                                                                                  \
	do {                                                                                                          \
		att_handle_compare_unsigned(ATT_COMP_NE, "EXPECT_UINT_NE(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (unsigned long long)(lhs), (unsigned long long)(rhs));                                    \
	} while (0)
#define ASSERT_UINT_LT(lhs, rhs)                                                                                 \
	do {                                                                                                         \
		att_handle_compare_unsigned(ATT_COMP_LT, "ASSERT_UINT_LT(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (unsigned long long)(lhs), (unsigned long long)(rhs));                                   \
	} while (0)
#define EXPECT_UINT_LT(lhs, rhs)                                                                                  \
	do {                                                                                                          \
		att_handle_compare_unsigned(ATT_COMP_LT, "EXPECT_UINT_LT(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (unsigned long long)(lhs), (unsigned long long)(rhs));                                    \
	} while (0)
#define ASSERT_UINT_LE(lhs, rhs)                                                                                 \
	do {                                                                                                         \
		att_handle_compare_unsigned(ATT_COMP_LE, "ASSERT_UINT_LE(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (unsigned long long)(lhs), (unsigned long long)(rhs));                                   \
	} while (0)
#define EXPECT_UINT_LE(lhs, rhs)                                                                                  \
	do {                                                                                                          \
		att_handle_compare_unsigned(ATT_COMP_LE, "EXPECT_UINT_LE(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (unsigned long long)(lhs), (unsigned long long)(rhs));                                    \
	} while (0)
#define ASSERT_UINT_GT(lhs, rhs)                                                                                 \
	do {                                                                                                         \
		att_handle_compare_unsigned(ATT_COMP_GT, "ASSERT_UINT_GT(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (unsigned long long)(lhs), (unsigned long long)(rhs));                                   \
	} while (0)
#define EXPECT_UINT_GT(lhs, rhs)                                                                                  \
	do {                                                                                                          \
		att_handle_compare_unsigned(ATT_COMP_GT, "EXPECT_UINT_GT(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (unsigned long long)(lhs), (unsigned long long)(rhs));                                    \
	} while (0)
#define ASSERT_UINT_GE(lhs, rhs)                                                                                 \
	do {                                                                                                         \
		att_handle_compare_unsigned(ATT_COMP_GE, "ASSERT_UINT_GE(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (unsigned long long)(lhs), (unsigned long long)(rhs));                                   \
	} while (0)
#define EXPECT_UINT_GE(lhs, rhs)                                                                                  \
	do {                                                                                                          \
		att_handle_compare_unsigned(ATT_COMP_GE, "EXPECT_UINT_GE(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (unsigned long long)(lhs), (unsigned long long)(rhs));                                    \
	} while (0)

/* C89-compatible explicit type macros - PTR (pointer) */
#define ASSERT_PTR_EQ(lhs, rhs)                                                                                \
	do {                                                                                                       \
		att_handle_compare_pointer(ATT_COMP_EQ, "ASSERT_PTR_EQ(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (const void *)(lhs), (const void *)(rhs));                                             \
	} while (0)
#define EXPECT_PTR_EQ(lhs, rhs)                                                                                 \
	do {                                                                                                        \
		att_handle_compare_pointer(ATT_COMP_EQ, "EXPECT_PTR_EQ(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (const void *)(lhs), (const void *)(rhs));                                              \
	} while (0)
#define ASSERT_PTR_NE(lhs, rhs)                                                                                \
	do {                                                                                                       \
		att_handle_compare_pointer(ATT_COMP_NE, "ASSERT_PTR_NE(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (const void *)(lhs), (const void *)(rhs));                                             \
	} while (0)
#define EXPECT_PTR_NE(lhs, rhs)                                                                                 \
	do {                                                                                                        \
		att_handle_compare_pointer(ATT_COMP_NE, "EXPECT_PTR_NE(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (const void *)(lhs), (const void *)(rhs));                                              \
	} while (0)
#define ASSERT_PTR_LT(lhs, rhs)                                                                                \
	do {                                                                                                       \
		att_handle_compare_pointer(ATT_COMP_LT, "ASSERT_PTR_LT(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (const void *)(lhs), (const void *)(rhs));                                             \
	} while (0)
#define EXPECT_PTR_LT(lhs, rhs)                                                                                 \
	do {                                                                                                        \
		att_handle_compare_pointer(ATT_COMP_LT, "EXPECT_PTR_LT(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (const void *)(lhs), (const void *)(rhs));                                              \
	} while (0)
#define ASSERT_PTR_LE(lhs, rhs)                                                                                \
	do {                                                                                                       \
		att_handle_compare_pointer(ATT_COMP_LE, "ASSERT_PTR_LE(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (const void *)(lhs), (const void *)(rhs));                                             \
	} while (0)
#define EXPECT_PTR_LE(lhs, rhs)                                                                                 \
	do {                                                                                                        \
		att_handle_compare_pointer(ATT_COMP_LE, "EXPECT_PTR_LE(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (const void *)(lhs), (const void *)(rhs));                                              \
	} while (0)
#define ASSERT_PTR_GT(lhs, rhs)                                                                                \
	do {                                                                                                       \
		att_handle_compare_pointer(ATT_COMP_GT, "ASSERT_PTR_GT(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (const void *)(lhs), (const void *)(rhs));                                             \
	} while (0)
#define EXPECT_PTR_GT(lhs, rhs)                                                                                 \
	do {                                                                                                        \
		att_handle_compare_pointer(ATT_COMP_GT, "EXPECT_PTR_GT(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (const void *)(lhs), (const void *)(rhs));                                              \
	} while (0)
#define ASSERT_PTR_GE(lhs, rhs)                                                                                \
	do {                                                                                                       \
		att_handle_compare_pointer(ATT_COMP_GE, "ASSERT_PTR_GE(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, \
			#lhs, #rhs, (const void *)(lhs), (const void *)(rhs));                                             \
	} while (0)
#define EXPECT_PTR_GE(lhs, rhs)                                                                                 \
	do {                                                                                                        \
		att_handle_compare_pointer(ATT_COMP_GE, "EXPECT_PTR_GE(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, \
			#lhs, #rhs, (const void *)(lhs), (const void *)(rhs));                                              \
	} while (0)

#define ASSERT_TRUE(expr)                                                                            \
	do {                                                                                             \
		att_handle_truth("ASSERT_TRUE(" #expr ")", __FILE__, __LINE__, true, !!(expr), true, #expr); \
	} while (0)
#define EXPECT_TRUE(expr)                                                                             \
	do {                                                                                              \
		att_handle_truth("EXPECT_TRUE(" #expr ")", __FILE__, __LINE__, false, !!(expr), true, #expr); \
	} while (0)
#define ASSERT_FALSE(expr)                                                                             \
	do {                                                                                               \
		att_handle_truth("ASSERT_FALSE(" #expr ")", __FILE__, __LINE__, true, !!(expr), false, #expr); \
	} while (0)
#define EXPECT_FALSE(expr)                                                                              \
	do {                                                                                                \
		att_handle_truth("EXPECT_FALSE(" #expr ")", __FILE__, __LINE__, false, !!(expr), false, #expr); \
	} while (0)

#define ASSERT_NULL(ptr)                                                                                                       \
	do {                                                                                                                       \
		att_handle_compare_pointer(ATT_COMP_EQ, "ASSERT_NULL(" #ptr ")", __FILE__, __LINE__, true, #ptr, "NULL", (ptr), NULL); \
	} while (0)
#define EXPECT_NULL(ptr)                                                                                                        \
	do {                                                                                                                        \
		att_handle_compare_pointer(ATT_COMP_EQ, "EXPECT_NULL(" #ptr ")", __FILE__, __LINE__, false, #ptr, "NULL", (ptr), NULL); \
	} while (0)
#define ASSERT_NOT_NULL(ptr)                                                                                                       \
	do {                                                                                                                           \
		att_handle_compare_pointer(ATT_COMP_NE, "ASSERT_NOT_NULL(" #ptr ")", __FILE__, __LINE__, true, #ptr, "NULL", (ptr), NULL); \
	} while (0)
#define EXPECT_NOT_NULL(ptr)                                                                                                        \
	do {                                                                                                                            \
		att_handle_compare_pointer(ATT_COMP_NE, "EXPECT_NOT_NULL(" #ptr ")", __FILE__, __LINE__, false, #ptr, "NULL", (ptr), NULL); \
	} while (0)

#define ATT_ASSERT(expr, ...)                                                             \
	do {                                                                                  \
		att_handle_custom_assert(__FILE__, __LINE__, true, !!(expr), #expr, __VA_ARGS__); \
	} while (0)
#define ATT_EXPECT(expr, ...)                                                              \
	do {                                                                                   \
		att_handle_custom_assert(__FILE__, __LINE__, false, !!(expr), #expr, __VA_ARGS__); \
	} while (0)

#define ASSERT_STREQ(lhs, rhs)                                                                                                  \
	do {                                                                                                                        \
		att_handle_string(ATT_COMP_EQ, "ASSERT_STREQ(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, #lhs, #rhs, (lhs), (rhs)); \
	} while (0)
#define EXPECT_STREQ(lhs, rhs)                                                                                                   \
	do {                                                                                                                         \
		att_handle_string(ATT_COMP_EQ, "EXPECT_STREQ(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, #lhs, #rhs, (lhs), (rhs)); \
	} while (0)
#define ASSERT_STRNE(lhs, rhs)                                                                                                  \
	do {                                                                                                                        \
		att_handle_string(ATT_COMP_NE, "ASSERT_STRNE(" #lhs ", " #rhs ")", __FILE__, __LINE__, true, #lhs, #rhs, (lhs), (rhs)); \
	} while (0)
#define EXPECT_STRNE(lhs, rhs)                                                                                                   \
	do {                                                                                                                         \
		att_handle_string(ATT_COMP_NE, "EXPECT_STRNE(" #lhs ", " #rhs ")", __FILE__, __LINE__, false, #lhs, #rhs, (lhs), (rhs)); \
	} while (0)

#define ASSERT_MEMEQ(lhs, rhs, size)                                                                                                  \
	do {                                                                                                                              \
		att_handle_memory("ASSERT_MEMEQ(" #lhs ", " #rhs ", " #size ")", __FILE__, __LINE__, true, (lhs), (rhs), (size), #lhs, #rhs); \
	} while (0)
#define EXPECT_MEMEQ(lhs, rhs, size)                                                                                                   \
	do {                                                                                                                               \
		att_handle_memory("EXPECT_MEMEQ(" #lhs ", " #rhs ", " #size ")", __FILE__, __LINE__, false, (lhs), (rhs), (size), #lhs, #rhs); \
	} while (0)

#define ASSERT_NEAR(lhs, rhs, eps)                                                                                                                             \
	do {                                                                                                                                                       \
		att_handle_near("ASSERT_NEAR(" #lhs ", " #rhs ", " #eps ")", __FILE__, __LINE__, true, (double)(lhs), (double)(rhs), (double)(eps), #lhs, #rhs, #eps); \
	} while (0)
#define EXPECT_NEAR(lhs, rhs, eps)                                                                                                                              \
	do {                                                                                                                                                        \
		att_handle_near("EXPECT_NEAR(" #lhs ", " #rhs ", " #eps ")", __FILE__, __LINE__, false, (double)(lhs), (double)(rhs), (double)(eps), #lhs, #rhs, #eps); \
	} while (0)

#define ASSERT_NEAR_REL(lhs, rhs, rel_eps)                                                                                                                                         \
	do {                                                                                                                                                                           \
		att_handle_near_rel("ASSERT_NEAR_REL(" #lhs ", " #rhs ", " #rel_eps ")", __FILE__, __LINE__, true, (double)(lhs), (double)(rhs), (double)(rel_eps), #lhs, #rhs, #rel_eps); \
	} while (0)
#define EXPECT_NEAR_REL(lhs, rhs, rel_eps)                                                                                                                                          \
	do {                                                                                                                                                                            \
		att_handle_near_rel("EXPECT_NEAR_REL(" #lhs ", " #rhs ", " #rel_eps ")", __FILE__, __LINE__, false, (double)(lhs), (double)(rhs), (double)(rel_eps), #lhs, #rhs, #rel_eps); \
	} while (0)

#define ASSERT_ULP_EQ(a, b, max_ulp)                                                                                 \
	do {                                                                                                             \
		att_handle_ulp_eq((double)(a), (double)(b), (int64_t)(max_ulp), __FILE__, __LINE__, #a, #b, #max_ulp, true); \
	} while (0)
#define EXPECT_ULP_EQ(a, b, max_ulp)                                                                                  \
	do {                                                                                                              \
		att_handle_ulp_eq((double)(a), (double)(b), (int64_t)(max_ulp), __FILE__, __LINE__, #a, #b, #max_ulp, false); \
	} while (0)

#define ATT_EXPECT_SUBTEST_FAILS(name, block, min, max)                                                                \
	do {                                                                                                               \
		att_captured att__captured = { 0 };                                                                            \
		int att__capture_active = (att_capture_begin() == 0);                                                          \
		att_subtest_scope *att__scope = att_subtest_scope_enter((name));                                               \
		att_result att__sub_result;                                                                                    \
		if (att_subtest_scope_protect(att__scope) == 0) {                                                              \
			block                                                                                                      \
		}                                                                                                              \
		att_status att__sub_status = att_subtest_scope_leave(att__scope, &att__sub_result);                            \
		if (att__capture_active) {                                                                                     \
			att_capture_end(&att__captured);                                                                           \
		}                                                                                                              \
		bool att__sub_expect_ok = att_handle_subtest_expect("ATT_EXPECT_SUBTEST_FAILS(" #name ", " #min ", " #max ")", \
			__FILE__, __LINE__, #name, (name), (min), (max), att__sub_status, &att__sub_result);                       \
		if (!att__sub_expect_ok && att__capture_active) {                                                              \
			att_replay_captured(&att__captured);                                                                       \
		}                                                                                                              \
		if (att__capture_active && att__captured.data) {                                                               \
			free(att__captured.data);                                                                                  \
		}                                                                                                              \
	} while (0)

#define ATT_CONCAT_INNER(a, b) a##b
#define ATT_CONCAT(a, b) ATT_CONCAT_INNER(a, b)
#define ATT_CONCAT3(a, b, c) ATT_CONCAT(ATT_CONCAT(a, b), c)
#define ATT_SUFFIX(name) _##name

/* Scoped info - cleanup attribute support varies by compiler */
#if defined(__GNUC__) || defined(__clang__)
#define SCOPED_INFO(fmt, ...)                \
	att_info_scope_push(fmt, ##__VA_ARGS__); \
	att_info_scope_t ATT_CONCAT(att__scope_, __LINE__) __attribute__((cleanup(att_info_scope_pop_impl))) = { 0 }
#else
/* MSVC: cleanup not supported, info still pushed but manual cleanup needed */
#define SCOPED_INFO(fmt, ...) \
	att_info_scope_push(fmt, ##__VA_ARGS__)
#endif

#if defined(__COUNTER__)
#define ATT_UNIQUE_ID(prefix) ATT_CONCAT(prefix, __COUNTER__)
#else
#define ATT_UNIQUE_ID(prefix) ATT_CONCAT(prefix, __LINE__)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define ATT_AUTOREG(fn)                                \
	static void fn(void) __attribute__((constructor)); \
	static void fn(void)
#elif defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
#define ATT_AUTOREG(fn)                                                               \
	static void __cdecl fn(void);                                                     \
	__declspec(allocate(".CRT$XCU")) void(__cdecl * ATT_CONCAT(fn, _ptr))(void) = fn; \
	static void __cdecl fn(void)
#else
#define ATT_AUTOREG(fn) static void fn(void)
#endif

#define ATT_TEST_FN_NAME(Suite, Name) ATT_CONCAT3(att_test_fn_, Suite, ATT_SUFFIX(Name))
#define ATT_REGISTER_FN_NAME(Suite, Name) ATT_CONCAT3(att_register_, Suite, ATT_SUFFIX(Name))
#define ATT_FIXTURE_BODY_FN_NAME(Fixture, Name) ATT_CONCAT3(att_fixture_body_, Fixture, ATT_SUFFIX(Name))
#define ATT_FIXTURE_SETUP_FN(Fixture) ATT_CONCAT(Fixture, _SetUp)
#define ATT_FIXTURE_TEARDOWN_FN(Fixture) ATT_CONCAT(Fixture, _TearDown)

#define ATT_TEST_IMPL(suite, name, fn_symbol, reg_symbol)              \
	static void fn_symbol(void);                                       \
	ATT_AUTOREG(reg_symbol)                                            \
	{                                                                  \
		att_register_test(suite, name, fn_symbol, __FILE__, __LINE__); \
	}                                                                  \
	static void fn_symbol(void)

#define ATT_TEST(Suite, Name) \
	ATT_TEST_IMPL(#Suite, #Name, ATT_TEST_FN_NAME(Suite, Name), ATT_REGISTER_FN_NAME(Suite, Name))

#define ATT_TEST_REF(Suite, Name) ATT_REGISTER_FN_NAME(Suite, Name)

#define ATT_FIXTURE_SETUP(Fixture)                                                                              \
	static void ATT_FIXTURE_SETUP_FN(Fixture)(Fixture * att_fixture);                                           \
	ATT_AUTOREG(ATT_CONCAT(att_fixture_setup_autoreg_, Fixture))                                                \
	{                                                                                                           \
		att_fixture_register_setup(#Fixture, sizeof(Fixture), (att_fixture_hook)ATT_FIXTURE_SETUP_FN(Fixture)); \
	}                                                                                                           \
	static void ATT_FIXTURE_SETUP_FN(Fixture)(Fixture * att_fixture)

#define ATT_FIXTURE_TEARDOWN(Fixture)                                                                                 \
	static void ATT_FIXTURE_TEARDOWN_FN(Fixture)(Fixture * att_fixture);                                              \
	ATT_AUTOREG(ATT_CONCAT(att_fixture_teardown_autoreg_, Fixture))                                                   \
	{                                                                                                                 \
		att_fixture_register_teardown(#Fixture, sizeof(Fixture), (att_fixture_hook)ATT_FIXTURE_TEARDOWN_FN(Fixture)); \
	}                                                                                                                 \
	static void ATT_FIXTURE_TEARDOWN_FN(Fixture)(Fixture * att_fixture)

#define ATT_FIXTURE(type) ((type *)att_fixture)

#define ATT_TEST_F_IMPL(suite, name, fixture_type, fn_symbol, body_symbol, reg_symbol)  \
	static void body_symbol(fixture_type *att_fixture);                                 \
	static void fn_symbol(void);                                                        \
	ATT_AUTOREG(reg_symbol)                                                             \
	{                                                                                   \
		att_register_test(suite, name, fn_symbol, __FILE__, __LINE__);                  \
	}                                                                                   \
	static void fn_symbol(void)                                                         \
	{                                                                                   \
		att_fixture_run(suite, sizeof(fixture_type), (att_fixture_body_fn)body_symbol); \
	}                                                                                   \
	static void body_symbol(fixture_type *att_fixture)

#define ATT_SKIP(reason)  \
	do {                  \
		att_skip(reason); \
	} while (0)

#define ATT_SKIP_IF(cond, reason) \
	do {                          \
		if (cond) {               \
			ATT_SKIP(reason);     \
		}                         \
	} while (0)

#define ATT_REGISTER_TESTS(...)                                                                     \
	do {                                                                                            \
		const att_register_fn att__manual_fns[] = { __VA_ARGS__ };                                  \
		att_register_manual(att__manual_fns, sizeof(att__manual_fns) / sizeof(att__manual_fns[0])); \
	} while (0)

#define TEST(Suite, Name) ATT_TEST(Suite, Name)
#define TEST_F(Fixture, Name)                                                  \
	ATT_TEST_F_IMPL(#Fixture, #Name, Fixture, ATT_TEST_FN_NAME(Fixture, Name), \
		ATT_FIXTURE_BODY_FN_NAME(Fixture, Name), ATT_REGISTER_FN_NAME(Fixture, Name))

#ifdef __cplusplus
}
#endif

#endif /* ATTEST_ATTEST_H */
