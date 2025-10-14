#ifndef ATTEST_INTERNAL_H
#define ATTEST_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "attest/attest.h"

/* ========================================================================
 * Platform Detection
 * ======================================================================== */
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	#define ATT_PLATFORM_WINDOWS
#else
	#define ATT_PLATFORM_POSIX
#endif

/* ========================================================================
 * Compiler Detection
 * ======================================================================== */
#if defined(_MSC_VER)
	#define ATT_COMPILER_MSVC
#elif defined(__GNUC__) || defined(__clang__)
	#define ATT_COMPILER_GCC_LIKE
#endif

/* ========================================================================
 * setjmp/longjmp Abstraction
 * ======================================================================== */
#include <setjmp.h>

#ifdef ATT_PLATFORM_POSIX
	#include <signal.h>
	typedef sigjmp_buf att_jmp_buf;
	#define att_setjmp(env) sigsetjmp((env), 1)
	#define att_longjmp(env, val) siglongjmp((env), (val))
#else
	/* Windows: Use standard C setjmp/longjmp */
	typedef jmp_buf att_jmp_buf;
	#define att_setjmp(env) setjmp(env)
	#define att_longjmp(env, val) longjmp((env), (val))
#endif

/* ========================================================================
 * Alignment Attribute
 * ======================================================================== */
#ifdef ATT_COMPILER_MSVC
	#define ATT_ALIGN(n) __declspec(align(n))
#elif defined(ATT_COMPILER_GCC_LIKE)
	#define ATT_ALIGN(n) __attribute__((aligned(n)))
#else
	#define ATT_ALIGN(n) _Alignas(n)
#endif

/* ========================================================================
 * Constructor Attribute (for auto-registration)
 * ======================================================================== */
#ifdef ATT_COMPILER_MSVC
	#define ATT_CONSTRUCTOR /* MSVC uses .CRT$XCU section, handled in attest.h */
#elif defined(ATT_COMPILER_GCC_LIKE)
	#define ATT_CONSTRUCTOR __attribute__((constructor))
#else
	#define ATT_CONSTRUCTOR /* Fallback: manual registration required */
#endif

/* ========================================================================
 * Cleanup Attribute (for scoped info)
 * ======================================================================== */
#ifdef ATT_COMPILER_MSVC
	#define ATT_CLEANUP(fn) /* MSVC: cleanup not supported, use manual management */
#elif defined(ATT_COMPILER_GCC_LIKE)
	#define ATT_CLEANUP(fn) __attribute__((cleanup(fn)))
#else
	#define ATT_CLEANUP(fn) /* Fallback: manual cleanup required */
#endif

typedef struct att_test_case {
	const char* suite;
	const char* name;
	const char* fullname;
	const char* file;
	int line;
	att_test_fn fn;
} att_test_case;

typedef struct att_registry {
	att_test_case* tests;
	size_t count;
	size_t capacity;
	bool frozen;
	const char* last_error;
	bool cleanup_registered;
} att_registry;

typedef enum att_output_format {
	ATT_OUTPUT_DEFAULT = 0,
	ATT_OUTPUT_TAP,
	ATT_OUTPUT_JUNIT
} att_output_format;

typedef struct att_cli_options {
	bool list_only;
	bool color_enabled;
	const char* filter_raw;
	char** filters;
	size_t filter_count;
	att_output_format format;
	char* output_path;
	int timeout_ms;
} att_cli_options;

typedef struct att_summary {
	int suites_total;
	int tests_total;
	int tests_selected;
	int tests_run;
	int tests_failed;
	int tests_skipped;
	int timeouts;
	int assertions_total;
	int failures_total;
} att_summary;

att_registry* att_registry_get(void);
void att_registry_finalize(void);
int att_registry_add(const char* suite, const char* name, att_test_fn fn, const char* file, int line, const char** error);
const char* att_registry_error(void);

int att_cli_parse(int argc, char** argv, att_cli_options* out_opts, char** err_msg);
void att_cli_dispose(att_cli_options* opts);
bool att_filter_match(const att_test_case* test, const att_cli_options* opts);
void att_print_list(const att_registry* registry, const att_cli_options* opts);
int att_run_tests(const att_registry* registry, const att_cli_options* opts, att_summary* summary);
void att_report_summary(const att_summary* summary, bool color_enabled);
void att_registry_cleanup(void);

#endif /* ATTEST_INTERNAL_H */
