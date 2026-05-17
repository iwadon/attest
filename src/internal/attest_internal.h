#ifndef ATTEST_INTERNAL_H
#define ATTEST_INTERNAL_H

/* Include attest.h first to ensure _POSIX_C_SOURCE is defined before any system headers */
#include "attest/attest.h"

/* ========================================================================
 * Platform Detection
 * ======================================================================== */
/* Note: ATT_PLATFORM_POSIX is defined in attest.h */
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#define ATT_PLATFORM_WINDOWS
#endif

/* Expose _POSIX_VERSION on POSIX systems. <unistd.h> is the canonical way to
 * surface it after _POSIX_C_SOURCE has been set in attest.h; without this the
 * thread-support detection below cannot tell POSIX environments apart from
 * freestanding ones and silently falls back to ATT_THREADS_NONE — which in
 * turn disables the entire parallel execution path. */
#if !defined(ATT_PLATFORM_WINDOWS) && !defined(ATT_PLATFORM_HUMAN68K)
#include <unistd.h>
#endif

/* ========================================================================
 * Thread Support Detection
 * ======================================================================== */
#if defined(ATT_PLATFORM_HUMAN68K)
/* Human68k: No thread support (single-threaded only) */
#define ATT_THREADS_NONE 1
#define ATT_THREAD_LOCAL /* No TLS support, use global variables */
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
/* C11 threads.h support */
#define ATT_THREADS_C11 1
#define ATT_THREAD_LOCAL _Thread_local
#elif defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112L
/* POSIX threads (pthread) support */
#define ATT_THREADS_POSIX 1
#if defined(__GNUC__) || defined(__clang__)
#define ATT_THREAD_LOCAL __thread
#else
#define ATT_THREAD_LOCAL _Thread_local
#endif
#elif defined(_WIN32) || defined(_WIN64)
/* Windows threads support */
#define ATT_THREADS_WIN32 1
#define ATT_THREAD_LOCAL __declspec(thread)
#else
/* No thread support */
#define ATT_THREADS_NONE 1
#define ATT_THREAD_LOCAL /* No TLS support, use global variables */
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
/* Note: att_jmp_buf, att_setjmp, att_longjmp are defined in attest.h */

/* ========================================================================
 * Alignment Attribute
 * ======================================================================== */
#ifdef ATT_COMPILER_MSVC
#define ATT_ALIGN(n) __declspec(align(n))
#elif defined(ATT_COMPILER_GCC_LIKE)
#define ATT_ALIGN(n) __attribute__((aligned(n)))
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define ATT_ALIGN(n) _Alignas(n)
#else
/* C99 or earlier without compiler-specific extensions: alignment hint omitted.
 * Safe on platforms without SIMD-style alignment requirements (e.g. m68k). */
#define ATT_ALIGN(n)
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
	const char *suite;
	const char *name;
	const char *fullname;
	const char *file;
	int line;
	att_test_fn fn;
} att_test_case;

typedef struct att_registry {
	att_test_case *tests;
	size_t count;
	size_t capacity;
	bool frozen;
	const char *last_error;
	bool cleanup_registered;
} att_registry;

typedef enum att_output_format {
	ATT_OUTPUT_DEFAULT = 0,
	ATT_OUTPUT_TAP,
	ATT_OUTPUT_JUNIT
} att_output_format;

typedef struct att_cli_options {
	bool list_only;
	bool help_requested;
	bool color_enabled;
	bool shuffle;
	unsigned int shuffle_seed;
	const char *filter_raw;
	char **filters;
	size_t filter_count;
	char **negative_filters;
	size_t negative_filter_count;
	att_output_format format;
	char *output_path;
	int timeout_ms;
	int jobs;
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

/* ========================================================================
 * Parallel Execution Data Structures
 * ======================================================================== */

/* Opaque types for parallel execution (implementation in attest_parallel.c) */
typedef struct att_parallel_result att_parallel_result;

#ifdef ATT_THREADS_POSIX
typedef struct att_worker_pool att_worker_pool;
typedef struct att_worker att_worker;
#endif

att_registry *att_registry_get(void);
void att_registry_finalize(void);
void att_registry_shuffle(unsigned int seed);
int att_registry_add(const char *suite, const char *name, att_test_fn fn, const char *file, int line, const char **error);
const char *att_registry_error(void);

int att_cli_parse(int argc, char **argv, att_cli_options *out_opts, char **err_msg);
void att_cli_dispose(att_cli_options *opts);
bool att_filter_match(const att_test_case *test, const att_cli_options *opts);
void att_print_list(const att_registry *registry, const att_cli_options *opts);
void att_print_help(const char *program_name);
int att_run_tests(const att_registry *registry, const att_cli_options *opts, att_summary *summary);
void att_report_summary(const att_summary *summary, bool color_enabled);
void att_registry_cleanup(void);

/* Format a "[ SKIPPED ] <name>\n  reason: <reason>\n" pair on stdout
 * using the default formatter. No-op for TAP and JUnit, which emit
 * skip information through their own paths. Shared between the
 * sequential runner (top-level skips) and att_subtest_scope_finalize
 * (subtest-scope skips) so both call sites format identically without
 * att_context_skip having to emit anything directly. */
void att_emit_skip_default(const char *fullname, const char *reason, att_output_format format);

/* ========================================================================
 * Parallel Execution API (Phase 2+)
 * ======================================================================== */

#ifdef ATT_THREADS_POSIX
int att_worker_pool_init(att_worker_pool *pool, const att_registry *registry,
	const att_cli_options *options, size_t total_tests);
void att_worker_pool_destroy(att_worker_pool *pool);
int att_run_tests_parallel(const att_registry *registry, const att_cli_options *opts, att_summary *summary);
#endif

#ifdef ATT_THREADS_NONE
/* Fallback stubs for non-threaded environments */
int att_worker_pool_init(void *pool, const void *registry,
	const void *options, size_t total_tests);
void att_worker_pool_destroy(void *pool);
#endif

#endif /* ATTEST_INTERNAL_H */
