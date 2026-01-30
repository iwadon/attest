#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attest/attest.h"
#include "internal/attest_context.h"
#include "internal/attest_internal.h"

/* Platform-specific includes */
#ifdef ATT_PLATFORM_POSIX
#include <signal.h>
#include <sys/time.h>
#endif

#ifdef ATT_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <process.h>
#include <windows.h>
#endif

typedef struct att_context_state {
	ATT_ALIGN(16)
	att_jmp_buf abort_env;
	const att_test_case *test;
	bool active;
	bool color_enabled;
	att_output_format format;
	att_test_result result;
	struct att_context_state *previous;
	att_context_phase phase;
	void *fixture_instance;
	size_t fixture_size;
	const char *fixture_name;
	att_fixture_hook fixture_teardown;
	bool fixture_active;
	bool fixture_teardown_ran;
	bool capture_failures;
	char *failure_buffer;
	size_t failure_size;
	size_t failure_capacity;
	bool timeout_triggered;
	int timeout_ms;
	char *info_stack[8];
	int info_stack_size;
#ifdef ATT_PLATFORM_WINDOWS
	HANDLE timeout_thread;
	HANDLE timeout_event;
	bool timeout_init_failed;
	unsigned int timeout_check_counter;
#endif
} ATT_ALIGN(16) att_context_state;

static ATT_THREAD_LOCAL ATT_ALIGN(16) att_context_state g_ctx_root;
static ATT_THREAD_LOCAL att_context_state *g_ctx;

static inline att_context_state *att_get_context(void)
{
	if (!g_ctx) {
		g_ctx = &g_ctx_root;
	}
	return g_ctx;
}

static char *att_dup_string(const char *text);
static const char *att_color_fail(void);
static const char *att_color_reset(void);
static bool att_context_failure_append_format(const char *fmt, ...);
static ATT_THREAD_LOCAL bool g_timeout_handler_installed;

#ifdef ATT_PLATFORM_POSIX
static void att_timeout_signal_handler(int signo)
{
	(void)signo;
	if (!g_ctx || !g_ctx->active) {
		return;
	}
	g_ctx->timeout_triggered = true;
	att_longjmp(g_ctx->abort_env, 1);
}
#endif

void att_context_begin(const att_test_case *test, bool color_enabled, att_output_format format)
{
	/* Ensure g_ctx is initialized by calling att_get_context() */
	att_get_context();

#ifdef ATT_PLATFORM_WINDOWS
	/* Clean up any leftover timeout resources before clearing context */
	if (g_ctx->timeout_thread || g_ctx->timeout_event) {
		att_context_timeout_stop();
	}
#endif
	memset(g_ctx, 0, sizeof(*g_ctx));
	g_ctx->test = test;
	g_ctx->color_enabled = color_enabled;
	g_ctx->format = format;
	g_ctx->active = true;
	g_ctx->phase = ATT_CONTEXT_PHASE_TEST;
}

int att_context_protect(void)
{
	return att_setjmp(g_ctx->abort_env);
}

void att_context_end(att_test_result *out_result)
{
	if (g_ctx->timeout_triggered) {
		g_ctx->result.timed_out = true;
		g_ctx->result.timeout_ms = g_ctx->timeout_ms;
		++g_ctx->result.fail_fatal;
		g_ctx->result.aborted = true;
		const att_test_case *test = att_context_current_test();
		const char *test_name = test ? test->fullname : "<unknown>";
		const char *fail_color = att_color_fail();
		const char *reset = att_color_reset();
		bool suppress_default_output = (g_ctx->format == ATT_OUTPUT_TAP || g_ctx->format == ATT_OUTPUT_JUNIT);
		if (!suppress_default_output) {
			fprintf(stderr, "%s[  FAILED  ]%s %s\n", fail_color, reset, test_name);
			fprintf(stderr, "  reason: timeout after %d ms\n", g_ctx->timeout_ms);
		}
		att_context_failure_append_format("[  FAILED  ] %s\n", test_name);
		att_context_failure_append_format("  reason: timeout after %d ms\n", g_ctx->timeout_ms);
		g_ctx->timeout_triggered = false;
	}
	att_context_fixture_cleanup();
	if (out_result) {
		*out_result = g_ctx->result;
		g_ctx->result.skip_reason = NULL;
		g_ctx->result.failure_log = NULL;
		out_result->failure_log = g_ctx->failure_buffer;
		g_ctx->failure_buffer = NULL;
		g_ctx->failure_size = 0;
		g_ctx->failure_capacity = 0;
	} else if (g_ctx->result.skip_reason) {
		free(g_ctx->result.skip_reason);
		g_ctx->result.skip_reason = NULL;
	}
	if (!out_result && g_ctx->failure_buffer) {
		free(g_ctx->failure_buffer);
		g_ctx->failure_buffer = NULL;
		g_ctx->failure_size = 0;
		g_ctx->failure_capacity = 0;
	}
	g_ctx->active = false;
	g_ctx->phase = ATT_CONTEXT_PHASE_NONE;
}

void att_context_record_assert(bool fatal, bool passed)
{
	if (!g_ctx->active) {
		return;
	}
#ifdef ATT_PLATFORM_WINDOWS
	/* Check for timeout on Windows (throttled to reduce overhead) */
	if (g_ctx->timeout_event) {
		/* Check every 32 assertions to minimize WaitForSingleObject overhead */
		if ((g_ctx->timeout_check_counter++ & 0x1F) == 0) {
			DWORD wait_result = WaitForSingleObject(g_ctx->timeout_event, 0);
			if (wait_result == WAIT_OBJECT_0 && g_ctx->timeout_triggered) {
				att_context_abort();
				return;
			}
		}
	}
#endif
	++g_ctx->result.assertions_total;
	if (!passed) {
		att_context_register_failure(fatal);
	}
}

void att_context_register_failure(bool fatal)
{
	if (!g_ctx->active) {
		return;
	}
	if (fatal) {
		++g_ctx->result.fail_fatal;
	} else {
		++g_ctx->result.fail_nonfatal;
	}
}

void att_context_abort(void)
{
	att_context_state *ctx = att_get_context();
	if (!ctx || !ctx->active) {
		return;
	}
	ctx->result.aborted = true;
	att_context_fixture_on_abort();
	/* Save timeout_ms before stopping, as stop clears it */
	int saved_timeout_ms = ctx->timeout_ms;
	att_context_timeout_stop();
	ctx->timeout_ms = saved_timeout_ms;

	/* Jump to the current context's setjmp point.
	   Do not walk up to parent contexts, as that causes stack frame misalignment.
	   The current context's abort_env was initialized by setjmp in the correct stack frame. */
	att_longjmp(ctx->abort_env, 1);
}

bool att_context_color_enabled(void)
{
	att_context_state *ctx = att_get_context();
	return ctx->color_enabled;
}

const att_test_case *att_context_current_test(void)
{
	att_context_state *ctx = att_get_context();
	return ctx->test;
}

void att_context_phase_set(att_context_phase phase)
{
	if (!g_ctx->active) {
		return;
	}
	g_ctx->phase = phase;
}

att_context_phase att_context_phase_current(void)
{
	att_context_state *ctx = att_get_context();
	return ctx->phase;
}

att_output_format att_context_get_format(void)
{
	att_context_state *ctx = att_get_context();
	return ctx->format;
}

void att_info_scope_push(const char *fmt, ...)
{
	if (!g_ctx || !g_ctx->active || g_ctx->info_stack_size >= 8) {
		return;
	}

	va_list args;
	va_start(args, fmt);
	int needed = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	if (needed < 0) {
		return;
	}

	size_t size = (size_t)needed + 1;
	char *buffer = malloc(size);
	if (!buffer) {
		return;
	}

	va_start(args, fmt);
	vsnprintf(buffer, size, fmt, args);
	va_end(args);

	g_ctx->info_stack[g_ctx->info_stack_size++] = buffer;
}

void att_info_scope_pop_impl(void *unused)
{
	(void)unused;
	if (g_ctx && g_ctx->active && g_ctx->info_stack_size > 0) {
		--g_ctx->info_stack_size;
		free(g_ctx->info_stack[g_ctx->info_stack_size]);
		g_ctx->info_stack[g_ctx->info_stack_size] = NULL;
	}
}

void att_context_fixture_enter(const char *fixture_name, size_t fixture_size, void *instance, att_fixture_hook teardown)
{
	if (!g_ctx->active) {
		return;
	}
	g_ctx->fixture_name = fixture_name;
	g_ctx->fixture_size = fixture_size;
	g_ctx->fixture_instance = instance;
	g_ctx->fixture_teardown = teardown;
	g_ctx->fixture_active = true;
	g_ctx->fixture_teardown_ran = false;
	g_ctx->phase = ATT_CONTEXT_PHASE_SETUP;
}

void att_context_fixture_mark_teardown_started(void)
{
	if (!g_ctx->active) {
		return;
	}
	g_ctx->fixture_teardown_ran = true;
}

void att_context_fixture_cleanup(void)
{
	if (!g_ctx->fixture_active) {
		return;
	}
	if (g_ctx->fixture_teardown && !g_ctx->fixture_teardown_ran) {
		g_ctx->fixture_teardown_ran = true;
		g_ctx->phase = ATT_CONTEXT_PHASE_TEARDOWN;
		g_ctx->fixture_teardown(g_ctx->fixture_instance);
	}
	void *instance = g_ctx->fixture_instance;
	g_ctx->fixture_instance = NULL;
	g_ctx->fixture_active = false;
	g_ctx->fixture_name = NULL;
	g_ctx->fixture_size = 0;
	g_ctx->fixture_teardown = NULL;
	g_ctx->fixture_teardown_ran = false;
	free(instance);
	g_ctx->phase = ATT_CONTEXT_PHASE_TEST;
}

void att_context_fixture_on_abort(void)
{
	if (!g_ctx->fixture_active) {
		return;
	}
	if (g_ctx->fixture_teardown && !g_ctx->fixture_teardown_ran) {
		void *instance = g_ctx->fixture_instance;
		att_fixture_hook teardown = g_ctx->fixture_teardown;
		g_ctx->fixture_teardown_ran = true;
		g_ctx->phase = ATT_CONTEXT_PHASE_TEARDOWN;
		teardown(instance);
	}
	void *instance = g_ctx->fixture_instance;
	g_ctx->fixture_instance = NULL;
	g_ctx->fixture_active = false;
	g_ctx->fixture_name = NULL;
	g_ctx->fixture_size = 0;
	g_ctx->fixture_teardown = NULL;
	g_ctx->fixture_teardown_ran = false;
	free(instance);
	g_ctx->phase = ATT_CONTEXT_PHASE_TEST;
}

void att_context_skip(const char *reason)
{
	att_context_state *ctx = att_get_context();
	if (!ctx || !ctx->active) {
		return;
	}

	if (ctx->result.skip_reason) {
		free(ctx->result.skip_reason);
		ctx->result.skip_reason = NULL;
	}

	if (reason) {
		ctx->result.skip_reason = att_dup_string(reason);
	}

	ctx->result.skipped = true;
	ctx->result.aborted = false;

	const att_test_case *test = att_context_current_test();
	const char *test_name = test ? test->fullname : "<unknown>";

	bool suppress_default_output = (ctx->format == ATT_OUTPUT_TAP || ctx->format == ATT_OUTPUT_JUNIT);
	if (!suppress_default_output) {
		printf("[  SKIPPED ] %s\n", test_name);
		printf("  reason: %s\n", reason ? reason : "(none)");
	}

	att_context_fixture_on_abort();

	/* Jump to the current context's setjmp point.
	   Do not walk up to parent contexts, as that causes stack frame misalignment.
	   The current context's abort_env was initialized by setjmp in the correct stack frame. */
	att_longjmp(ctx->abort_env, 1);
}

void att_context_capture_failures(bool enabled)
{
	if (!g_ctx->active) {
		return;
	}
	if (enabled) {
		g_ctx->capture_failures = true;
		g_ctx->failure_size = 0;
		if (g_ctx->failure_buffer) {
			g_ctx->failure_buffer[0] = '\0';
		}
		return;
	}
	if (g_ctx->failure_buffer) {
		free(g_ctx->failure_buffer);
		g_ctx->failure_buffer = NULL;
	}
	g_ctx->failure_size = 0;
	g_ctx->failure_capacity = 0;
	g_ctx->capture_failures = false;
}

#ifdef ATT_PLATFORM_POSIX
/* POSIX implementation: signal-based timeout using setitimer */
static void att_timeout_install_handler(void)
{
	if (g_timeout_handler_installed) {
		return;
	}
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = att_timeout_signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGALRM, &sa, NULL) == 0) {
		g_timeout_handler_installed = true;
	}
}

void att_context_timeout_start(int timeout_ms)
{
	if (!g_ctx->active || timeout_ms <= 0) {
		return;
	}
	att_timeout_install_handler();
	g_ctx->timeout_triggered = false;
	g_ctx->timeout_ms = timeout_ms;
	struct itimerval timer;
	memset(&timer, 0, sizeof(timer));
	timer.it_value.tv_sec = timeout_ms / 1000;
	timer.it_value.tv_usec = (timeout_ms % 1000) * 1000;
	setitimer(ITIMER_REAL, &timer, NULL);
}

void att_context_timeout_stop(void)
{
	struct itimerval timer;
	memset(&timer, 0, sizeof(timer));
	setitimer(ITIMER_REAL, &timer, NULL);
	if (g_ctx) {
		g_ctx->timeout_ms = 0;
	}
}
#elif defined(ATT_PLATFORM_HUMAN68K)
/* Human68k: Timeout feature not supported (no setitimer/sigaction available) */
void att_context_timeout_start(int timeout_ms)
{
	(void)timeout_ms;
	/* No-op: timeout not supported on Human68k */
}

void att_context_timeout_stop(void)
{
	/* No-op: timeout not supported on Human68k */
}
#else
/* Windows: Timeout feature using threads */
static unsigned __stdcall att_timeout_thread_proc(void *arg)
{
	int timeout_ms = (int)(intptr_t)arg;

	/* Use WaitForSingleObject instead of Sleep to allow early cancellation */
	if (g_ctx && g_ctx->timeout_event) {
		DWORD wait_result = WaitForSingleObject(g_ctx->timeout_event, (DWORD)timeout_ms);

		/* WAIT_TIMEOUT means the timeout expired (expected path) */
		/* WAIT_OBJECT_0 means the event was signaled (early cancellation) */
		if (wait_result == WAIT_TIMEOUT && g_ctx->active && !g_ctx->timeout_triggered) {
			g_ctx->timeout_triggered = true;
			/* Signal the event to indicate timeout occurred */
			SetEvent(g_ctx->timeout_event);
		}
	}

	return 0;
}

void att_context_timeout_start(int timeout_ms)
{
	if (!g_ctx->active || timeout_ms <= 0) {
		return;
	}

	/* Clean up any existing timeout */
	att_context_timeout_stop();

	g_ctx->timeout_triggered = false;
	g_ctx->timeout_ms = timeout_ms;
	g_ctx->timeout_init_failed = false;
	g_ctx->timeout_check_counter = 0;

	/* Create event for timeout signaling */
	g_ctx->timeout_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!g_ctx->timeout_event) {
		fprintf(stderr, "Warning: Failed to create timeout event. Timeout feature disabled.\n");
		g_ctx->timeout_init_failed = true;
		g_ctx->timeout_ms = 0;
		return;
	}

	/* Create timeout thread */
	g_ctx->timeout_thread = (HANDLE)_beginthreadex(
		NULL,
		0,
		att_timeout_thread_proc,
		(void *)(intptr_t)timeout_ms,
		0,
		NULL);

	if (!g_ctx->timeout_thread) {
		fprintf(stderr, "Warning: Failed to create timeout thread. Timeout feature disabled.\n");
		CloseHandle(g_ctx->timeout_event);
		g_ctx->timeout_event = NULL;
		g_ctx->timeout_init_failed = true;
		g_ctx->timeout_ms = 0;
	}
}

void att_context_timeout_stop(void)
{
	if (!g_ctx) {
		return;
	}

	if (g_ctx->timeout_thread) {
		/* Signal the event to wake up the timeout thread early */
		if (g_ctx->timeout_event) {
			SetEvent(g_ctx->timeout_event);
		}

		/* Wait for thread to complete with reasonable timeout (1 second should be enough) */
		DWORD wait_result = WaitForSingleObject(g_ctx->timeout_thread, 1000);
		if (wait_result == WAIT_TIMEOUT) {
			/* Thread didn't finish in time - this shouldn't happen with the new implementation
			   but we handle it gracefully to avoid hanging */
			fprintf(stderr, "Warning: Timeout thread did not terminate in time\n");
		}
		CloseHandle(g_ctx->timeout_thread);
		g_ctx->timeout_thread = NULL;
	}

	if (g_ctx->timeout_event) {
		CloseHandle(g_ctx->timeout_event);
		g_ctx->timeout_event = NULL;
	}

	g_ctx->timeout_ms = 0;
}
#endif
typedef enum {
	ATT_COLOR_FAIL,
	ATT_COLOR_FILE,
	ATT_COLOR_RESET,
	ATT_COLOR_BG_DARK_RED,
	ATT_COLOR_BG_DARK_GREEN,
	ATT_COLOR_BG_RED,
	ATT_COLOR_BG_GREEN,
	ATT_COLOR_COUNT
} att_color_code;

static const char *const att_color_codes[ATT_COLOR_COUNT] = {
	[ATT_COLOR_FAIL] = "\033[31m",
	[ATT_COLOR_FILE] = "\033[90m",
	[ATT_COLOR_RESET] = "\033[0m",
	[ATT_COLOR_BG_DARK_RED] = "\033[48;2;80;20;20m",
	[ATT_COLOR_BG_DARK_GREEN] = "\033[48;2;20;60;20m",
	[ATT_COLOR_BG_RED] = "\033[48;2;120;30;30m",
	[ATT_COLOR_BG_GREEN] = "\033[48;2;30;90;30m",
};

static const char *att_color(att_color_code code)
{
	return att_context_color_enabled() ? att_color_codes[code] : "";
}

static const char *att_color_fail(void) { return att_color(ATT_COLOR_FAIL); }
static const char *att_color_file(void) { return att_color(ATT_COLOR_FILE); }
static const char *att_color_reset(void) { return att_color(ATT_COLOR_RESET); }
static const char *att_color_bg_dark_red(void) { return att_color(ATT_COLOR_BG_DARK_RED); }
static const char *att_color_bg_dark_green(void) { return att_color(ATT_COLOR_BG_DARK_GREEN); }
static const char *att_color_bg_red(void) { return att_color(ATT_COLOR_BG_RED); }
static const char *att_color_bg_green(void) { return att_color(ATT_COLOR_BG_GREEN); }

typedef struct att_formatted {
	char buffer[128];
	const char *text;
} att_formatted;

#define ATT_DEFINE_FORMATTER(name, type, fmt_spec)                 \
	static att_formatted att_format_##name(type value)             \
	{                                                              \
		att_formatted fmt;                                         \
		snprintf(fmt.buffer, sizeof(fmt.buffer), fmt_spec, value); \
		fmt.text = fmt.buffer;                                     \
		return fmt;                                                \
	}

ATT_DEFINE_FORMATTER(signed, long long, "%lld")
ATT_DEFINE_FORMATTER(unsigned, unsigned long long, "%llu")
ATT_DEFINE_FORMATTER(double, double, "%.9g")
ATT_DEFINE_FORMATTER(long_double, long double, "%.18Lg")

static att_formatted att_format_pointer(const void *value)
{
	att_formatted fmt;
	if (!value) {
		fmt.text = "(null)";
		fmt.buffer[0] = '\0';
		return fmt;
	}
	snprintf(fmt.buffer, sizeof(fmt.buffer), "0x%" PRIxPTR, (uintptr_t)value);
	fmt.text = fmt.buffer;
	return fmt;
}

static att_formatted att_format_bool(bool value)
{
	att_formatted fmt;
	fmt.text = value ? "true" : "false";
	fmt.buffer[0] = '\0';
	return fmt;
}

static att_formatted att_format_string(const char *value)
{
	att_formatted fmt;
	if (!value) {
		fmt.text = "(null)";
		fmt.buffer[0] = '\0';
		return fmt;
	}
	snprintf(fmt.buffer, sizeof(fmt.buffer), "\"%s\"", value);
	fmt.text = fmt.buffer;
	return fmt;
}

static void att_build_expr(char *buffer, size_t size, const char *lhs_expr, const att_formatted *lhs_value, const char *rhs_expr, const att_formatted *rhs_value)
{
	snprintf(buffer, size, "%s=%s, %s=%s", lhs_expr, lhs_value->text, rhs_expr, rhs_value->text);
}

static char *att_dup_string(const char *text)
{
	if (!text) {
		return NULL;
	}
	size_t length = strlen(text);
	char *copy = malloc(length + 1);
	if (!copy) {
		return NULL;
	}
	memcpy(copy, text, length + 1);
	return copy;
}

static bool att_context_failure_append(const char *data, size_t length)
{
	if (!g_ctx->capture_failures || !data || length == 0) {
		return true;
	}
	size_t required = g_ctx->failure_size + length + 1;
	if (required > g_ctx->failure_capacity) {
		size_t new_capacity = g_ctx->failure_capacity ? g_ctx->failure_capacity * 2 : 256;
		while (new_capacity < required) {
			new_capacity *= 2;
		}
		char *grown = realloc(g_ctx->failure_buffer, new_capacity);
		if (!grown) {
			return false;
		}
		g_ctx->failure_buffer = grown;
		g_ctx->failure_capacity = new_capacity;
	}
	memcpy(g_ctx->failure_buffer + g_ctx->failure_size, data, length);
	g_ctx->failure_size += length;
	g_ctx->failure_buffer[g_ctx->failure_size] = '\0';
	return true;
}

static bool att_context_failure_append_format(const char *fmt, ...)
{
	if (!g_ctx->capture_failures) {
		return true;
	}
	va_list args;
	va_start(args, fmt);
	int needed = vsnprintf(NULL, 0, fmt, args);
	va_end(args);
	if (needed <= 0) {
		return false;
	}
	size_t length = (size_t)needed;
	char *buffer = malloc(length + 1);
	if (!buffer) {
		return false;
	}
	va_start(args, fmt);
	vsnprintf(buffer, length + 1, fmt, args);
	va_end(args);
	bool ok = att_context_failure_append(buffer, length);
	free(buffer);
	return ok;
}

static const char *att_phase_tag(att_context_phase phase)
{
	switch (phase) {
	case ATT_CONTEXT_PHASE_SETUP:
		return "setup";
	case ATT_CONTEXT_PHASE_TEARDOWN:
		return "teardown";
	default:
		return NULL;
	}
}

typedef struct att_failure_context {
	const char *test_name;
	const char *phase;
	const char *fail_color;
	const char *file_color;
	const char *reset;
	FILE *out;
	bool suppress;
} att_failure_context;

static att_failure_context att_failure_begin(void)
{
	const att_test_case *test = att_context_current_test();
	att_failure_context fc = {
		.test_name = test ? test->fullname : "<unknown>",
		.phase = att_phase_tag(att_context_phase_current()),
		.fail_color = att_color_fail(),
		.file_color = att_color_file(),
		.reset = att_color_reset(),
		.out = stderr,
		.suppress = (g_ctx && g_ctx->active && (g_ctx->format == ATT_OUTPUT_TAP || g_ctx->format == ATT_OUTPUT_JUNIT)),
	};

	if (!fc.suppress) {
		fprintf(fc.out, "%s[  FAILED  ]%s %s", fc.fail_color, fc.reset, fc.test_name);
		if (fc.phase) {
			fprintf(fc.out, " (%s)", fc.phase);
		}
		fprintf(fc.out, "\n");
	}
	if (fc.phase) {
		att_context_failure_append_format("[  FAILED  ] %s (%s)\n", fc.test_name, fc.phase);
	} else {
		att_context_failure_append_format("[  FAILED  ] %s\n", fc.test_name);
	}

	return fc;
}

static void att_print_info_stack_and_location(const att_failure_context *fc, const char *assertion, const char *file, int line, bool fatal)
{
	for (int i = 0; i < g_ctx->info_stack_size; ++i) {
		att_context_failure_append_format("  context: %s\n", g_ctx->info_stack[i]);
	}
	if (!fc->suppress) {
		for (int i = 0; i < g_ctx->info_stack_size; ++i) {
			fprintf(fc->out, "  context: %s\n", g_ctx->info_stack[i]);
		}
		fprintf(fc->out, "%s  ", fc->file_color);
		if (fc->phase) {
			fprintf(fc->out, "(%s) ", fc->phase);
		}
		fprintf(fc->out, "%s:%d: %s failed%s%s\n",
			file,
			line,
			assertion,
			fatal ? " (fatal)." : ".",
			fc->reset);
	}
	if (fc->phase) {
		att_context_failure_append_format("  (%s) %s:%d: %s failed%s\n",
			fc->phase,
			file,
			line,
			assertion,
			fatal ? " (fatal)." : ".");
	} else {
		att_context_failure_append_format("  %s:%d: %s failed%s\n",
			file,
			line,
			assertion,
			fatal ? " (fatal)." : ".");
	}
}

static void att_report_failure(bool fatal, const char *assertion, const char *file, int line, const char *expected, const char *actual, const char *expr_detail, const char *extra_label, const char *extra_value)
{
	att_failure_context fc = att_failure_begin();
	att_print_info_stack_and_location(&fc, assertion, file, line, fatal);

	if (!fc.suppress) {
		fprintf(fc.out, "    expected: %s\n", expected);
	}
	att_context_failure_append_format("    expected: %s\n", expected);
	if (!fc.suppress) {
		fprintf(fc.out, "      actual: %s\n", actual);
	}
	att_context_failure_append_format("      actual: %s\n", actual);
	if (extra_label && extra_value) {
		if (!fc.suppress) {
			fprintf(fc.out, "    %s: %s\n", extra_label, extra_value);
		}
		att_context_failure_append_format("    %s: %s\n", extra_label, extra_value);
	}
	if (!fc.suppress) {
		fprintf(fc.out, "    expr: %s\n", expr_detail);
	}
	att_context_failure_append_format("    expr: %s\n", expr_detail);
}

#define ATT_DEFINE_COMPARE(name, type)                         \
	static bool att_compare_##name(int op, type lhs, type rhs) \
	{                                                          \
		switch (op) {                                          \
		case ATT_COMP_EQ:                                      \
			return lhs == rhs;                                 \
		case ATT_COMP_NE:                                      \
			return lhs != rhs;                                 \
		case ATT_COMP_LT:                                      \
			return lhs < rhs;                                  \
		case ATT_COMP_LE:                                      \
			return lhs <= rhs;                                 \
		case ATT_COMP_GT:                                      \
			return lhs > rhs;                                  \
		case ATT_COMP_GE:                                      \
			return lhs >= rhs;                                 \
		default:                                               \
			return false;                                      \
		}                                                      \
	}

ATT_DEFINE_COMPARE(values, long long)
ATT_DEFINE_COMPARE(unsigned_values, unsigned long long)

void att_handle_compare_signed(int op, const char *assertion, const char *file, int line, bool fatal, const char *lhs_expr, const char *rhs_expr, long long lhs, long long rhs)
{
	bool passed = att_compare_values(op, lhs, rhs);
	att_context_record_assert(fatal, passed);
	if (passed) {
		return;
	}

	att_formatted lhs_fmt = att_format_signed(lhs);
	if (lhs_fmt.text == NULL || lhs_fmt.buffer[0] != '\0') {
		lhs_fmt.text = lhs_fmt.buffer;
	}
	att_formatted rhs_fmt = att_format_signed(rhs);
	if (rhs_fmt.text == NULL || rhs_fmt.buffer[0] != '\0') {
		rhs_fmt.text = rhs_fmt.buffer;
	}
	char expr[256];
	att_build_expr(expr, sizeof(expr), lhs_expr, &lhs_fmt, rhs_expr, &rhs_fmt);
	att_report_failure(fatal, assertion, file, line, lhs_fmt.text, rhs_fmt.text, expr, NULL, NULL);
	if (fatal) {
		att_context_abort();
	}
}

void att_handle_compare_unsigned(int op, const char *assertion, const char *file, int line, bool fatal, const char *lhs_expr, const char *rhs_expr, unsigned long long lhs, unsigned long long rhs)
{
	bool passed = att_compare_unsigned_values(op, lhs, rhs);
	att_context_record_assert(fatal, passed);
	if (passed) {
		return;
	}

	att_formatted lhs_fmt = att_format_unsigned(lhs);
	if (lhs_fmt.text == NULL || lhs_fmt.buffer[0] != '\0') {
		lhs_fmt.text = lhs_fmt.buffer;
	}
	att_formatted rhs_fmt = att_format_unsigned(rhs);
	if (rhs_fmt.text == NULL || rhs_fmt.buffer[0] != '\0') {
		rhs_fmt.text = rhs_fmt.buffer;
	}
	char expr[256];
	att_build_expr(expr, sizeof(expr), lhs_expr, &lhs_fmt, rhs_expr, &rhs_fmt);
	att_report_failure(fatal, assertion, file, line, lhs_fmt.text, rhs_fmt.text, expr, NULL, NULL);
	if (fatal) {
		att_context_abort();
	}
}

ATT_DEFINE_COMPARE(double_values, double)

void att_handle_compare_double(int op, const char *assertion, const char *file, int line, bool fatal, const char *lhs_expr, const char *rhs_expr, double lhs, double rhs)
{
	bool passed = att_compare_double_values(op, lhs, rhs);
	att_context_record_assert(fatal, passed);
	if (passed) {
		return;
	}

	att_formatted lhs_fmt = att_format_double(lhs);
	if (lhs_fmt.text == NULL || lhs_fmt.buffer[0] != '\0') {
		lhs_fmt.text = lhs_fmt.buffer;
	}
	att_formatted rhs_fmt = att_format_double(rhs);
	if (rhs_fmt.text == NULL || rhs_fmt.buffer[0] != '\0') {
		rhs_fmt.text = rhs_fmt.buffer;
	}
	char expr[256];
	att_build_expr(expr, sizeof(expr), lhs_expr, &lhs_fmt, rhs_expr, &rhs_fmt);
	att_report_failure(fatal, assertion, file, line, lhs_fmt.text, rhs_fmt.text, expr, NULL, NULL);
	if (fatal) {
		att_context_abort();
	}
}

ATT_DEFINE_COMPARE(long_double_values, long double)

void att_handle_compare_long_double(int op, const char *assertion, const char *file, int line, bool fatal, const char *lhs_expr, const char *rhs_expr, long double lhs, long double rhs)
{
	bool passed = att_compare_long_double_values(op, lhs, rhs);
	att_context_record_assert(fatal, passed);
	if (passed) {
		return;
	}

	att_formatted lhs_fmt = att_format_long_double(lhs);
	if (lhs_fmt.text == NULL || lhs_fmt.buffer[0] != '\0') {
		lhs_fmt.text = lhs_fmt.buffer;
	}
	att_formatted rhs_fmt = att_format_long_double(rhs);
	if (rhs_fmt.text == NULL || rhs_fmt.buffer[0] != '\0') {
		rhs_fmt.text = rhs_fmt.buffer;
	}
	char expr[256];
	att_build_expr(expr, sizeof(expr), lhs_expr, &lhs_fmt, rhs_expr, &rhs_fmt);
	att_report_failure(fatal, assertion, file, line, lhs_fmt.text, rhs_fmt.text, expr, NULL, NULL);
	if (fatal) {
		att_context_abort();
	}
}

void att_handle_compare_pointer(int op, const char *assertion, const char *file, int line, bool fatal, const char *lhs_expr, const char *rhs_expr, const void *lhs, const void *rhs)
{
	uintptr_t lhs_addr = (uintptr_t)lhs;
	uintptr_t rhs_addr = (uintptr_t)rhs;
	bool passed;

	switch (op) {
	case ATT_COMP_EQ:
		passed = lhs_addr == rhs_addr;
		break;
	case ATT_COMP_NE:
		passed = lhs_addr != rhs_addr;
		break;
	case ATT_COMP_LT:
		passed = lhs_addr < rhs_addr;
		break;
	case ATT_COMP_LE:
		passed = lhs_addr <= rhs_addr;
		break;
	case ATT_COMP_GT:
		passed = lhs_addr > rhs_addr;
		break;
	case ATT_COMP_GE:
		passed = lhs_addr >= rhs_addr;
		break;
	default:
		passed = false;
		break;
	}

	att_context_record_assert(fatal, passed);
	if (passed) {
		return;
	}

	att_formatted lhs_fmt = att_format_pointer(lhs);
	if (lhs_fmt.text == NULL || lhs_fmt.buffer[0] != '\0') {
		lhs_fmt.text = lhs_fmt.buffer;
	}
	att_formatted rhs_fmt = att_format_pointer(rhs);
	if (rhs_fmt.text == NULL || rhs_fmt.buffer[0] != '\0') {
		rhs_fmt.text = rhs_fmt.buffer;
	}
	char expr[256];
	att_build_expr(expr, sizeof(expr), lhs_expr, &lhs_fmt, rhs_expr, &rhs_fmt);
	att_report_failure(fatal, assertion, file, line, lhs_fmt.text, rhs_fmt.text, expr, NULL, NULL);
	if (fatal) {
		att_context_abort();
	}
}

void att_handle_truth(const char *assertion, const char *file, int line, bool fatal, bool value, bool expect_true, const char *expr)
{
	bool passed = (value == expect_true);
	att_context_record_assert(fatal, passed);
	if (passed) {
		return;
	}

	att_formatted expected_fmt = att_format_bool(expect_true);
	att_formatted actual_fmt = att_format_bool(value);
	char expr_buffer[256];
	snprintf(expr_buffer, sizeof(expr_buffer), "%s=%s", expr, actual_fmt.text);
	att_report_failure(fatal, assertion, file, line, expected_fmt.text, actual_fmt.text, expr_buffer, NULL, NULL);
	if (fatal) {
		att_context_abort();
	}
}

void att_handle_custom_assert(const char *file, int line, bool fatal, bool value, const char *expr, const char *fmt, ...)
{
	bool passed = value;
	att_context_record_assert(fatal, passed);
	if (passed) {
		return;
	}

	char message[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(message, sizeof(message), fmt, args);
	va_end(args);

	const char *assertion_name = fatal ? "ATT_ASSERT" : "ATT_EXPECT";
	char assertion_text[128];
	snprintf(assertion_text, sizeof(assertion_text), "%s(%s, ...)", assertion_name, expr);

	att_report_failure(fatal, assertion_text, file, line, "expression evaluates to true", message, expr, NULL, NULL);
	if (fatal) {
		att_context_abort();
	}
}

static bool att_strings_equal(const char *lhs, const char *rhs)
{
	if (lhs == NULL && rhs == NULL) {
		return true;
	}
	if (lhs == NULL || rhs == NULL) {
		return false;
	}
	return strcmp(lhs, rhs) == 0;
}

typedef struct att_string_lines {
	char **lines;
	size_t count;
} att_string_lines;

static att_string_lines att_split_lines(const char *str)
{
	att_string_lines result = { NULL, 0 };
	if (!str) {
		return result;
	}

	size_t capacity = 16;
	result.lines = malloc(capacity * sizeof(char *));
	if (!result.lines) {
		return result;
	}

	const char *line_start = str;
	const char *pos = str;

	while (*pos) {
		if (*pos == '\n') {
			size_t len = pos - line_start;
			char *line = malloc(len + 1);
			if (line) {
				memcpy(line, line_start, len);
				line[len] = '\0';
				if (result.count >= capacity) {
					capacity *= 2;
					char **grown = realloc(result.lines, capacity * sizeof(char *));
					if (!grown) {
						free(line);
						break;
					}
					result.lines = grown;
				}
				result.lines[result.count++] = line;
			}
			line_start = pos + 1;
		}
		pos++;
	}

	if (line_start != pos) {
		size_t len = pos - line_start;
		char *line = malloc(len + 1);
		if (line) {
			memcpy(line, line_start, len);
			line[len] = '\0';
			if (result.count >= capacity) {
				capacity *= 2;
				char **grown = realloc(result.lines, capacity * sizeof(char *));
				if (grown) {
					result.lines = grown;
					result.lines[result.count++] = line;
				} else {
					free(line);
				}
			} else {
				result.lines[result.count++] = line;
			}
		}
	}

	return result;
}

static void att_free_lines(att_string_lines *lines)
{
	if (!lines || !lines->lines) {
		return;
	}
	for (size_t i = 0; i < lines->count; i++) {
		free(lines->lines[i]);
	}
	free(lines->lines);
	lines->lines = NULL;
	lines->count = 0;
}

static size_t att_find_first_diff(const char *lhs, const char *rhs)
{
	if (!lhs || !rhs) {
		return 0;
	}
	size_t i = 0;
	while (lhs[i] && rhs[i] && lhs[i] == rhs[i]) {
		i++;
	}
	return i;
}

static void att_print_line_diff(FILE *out, size_t line_num, const char *expected, const char *actual)
{
	const char *bg_dark_red = att_color_bg_dark_red();
	const char *bg_dark_green = att_color_bg_dark_green();
	const char *bg_red = att_color_bg_red();
	const char *bg_green = att_color_bg_green();
	const char *reset = att_color_reset();
	bool color = att_context_color_enabled();

	if (expected && actual && strcmp(expected, actual) == 0) {
		fprintf(out, "  %3zu  %s\n", line_num, expected);
		return;
	}

	if (expected && actual) {
		size_t diff_pos = att_find_first_diff(expected, actual);
		size_t exp_len = strlen(expected);
		size_t act_len = strlen(actual);

		fprintf(out, "  %3zu -%s", line_num, color ? bg_dark_red : "");
		for (size_t i = 0; i < exp_len; i++) {
			if (color && i >= diff_pos && (i >= act_len || expected[i] != actual[i])) {
				fprintf(out, "%s%c%s%s", bg_red, expected[i], reset, bg_dark_red);
			} else if (color) {
				fprintf(out, "%s%c", bg_dark_red, expected[i]);
			} else {
				fputc(expected[i], out);
			}
		}
		fprintf(out, "%s\n", reset);

		fprintf(out, "  %3zu +%s", line_num, color ? bg_dark_green : "");
		for (size_t i = 0; i < act_len; i++) {
			if (color && i >= diff_pos && (i >= exp_len || expected[i] != actual[i])) {
				fprintf(out, "%s%c%s%s", bg_green, actual[i], reset, bg_dark_green);
			} else if (color) {
				fprintf(out, "%s%c", bg_dark_green, actual[i]);
			} else {
				fputc(actual[i], out);
			}
		}
		fprintf(out, "%s\n", reset);
	} else if (expected) {
		fprintf(out, "  %3zu -%s%s%s\n", line_num, color ? bg_dark_red : "", expected, reset);
	} else if (actual) {
		fprintf(out, "  %3zu +%s%s%s\n", line_num, color ? bg_dark_green : "", actual, reset);
	}
}

static void att_format_string_diff(FILE *out, const char *expected, const char *actual)
{
	att_string_lines exp_lines = att_split_lines(expected);
	att_string_lines act_lines = att_split_lines(actual);

	size_t max_lines = exp_lines.count > act_lines.count ? exp_lines.count : act_lines.count;

	if (max_lines > 1) {
		fprintf(out, "    String comparison failed (%zu lines):\n", max_lines);
	}

	for (size_t i = 0; i < max_lines; i++) {
		const char *exp = i < exp_lines.count ? exp_lines.lines[i] : NULL;
		const char *act = i < act_lines.count ? act_lines.lines[i] : NULL;
		att_print_line_diff(out, i + 1, exp, act);
	}

	att_free_lines(&exp_lines);
	att_free_lines(&act_lines);
}

void att_handle_string(int op, const char *assertion, const char *file, int line, bool fatal, const char *lhs_expr, const char *rhs_expr, const char *lhs, const char *rhs)
{
	bool eq = att_strings_equal(lhs, rhs);
	bool passed = (op == ATT_COMP_EQ) ? eq : !eq;

	att_context_record_assert(fatal, passed);
	if (passed) {
		return;
	}

	att_failure_context fc = att_failure_begin();
	bool has_newline = (lhs && strchr(lhs, '\n')) || (rhs && strchr(rhs, '\n'));
	att_print_info_stack_and_location(&fc, assertion, file, line, fatal);

	if (has_newline && !fc.suppress) {
		att_format_string_diff(fc.out, lhs, rhs);
	} else {
		att_formatted lhs_fmt = att_format_string(lhs);
		if (lhs_fmt.text == NULL || lhs_fmt.buffer[0] != '\0') {
			lhs_fmt.text = lhs_fmt.buffer;
		}
		att_formatted rhs_fmt = att_format_string(rhs);
		if (rhs_fmt.text == NULL || rhs_fmt.buffer[0] != '\0') {
			rhs_fmt.text = rhs_fmt.buffer;
		}
		if (!fc.suppress) {
			fprintf(fc.out, "    expected: %s\n", lhs_fmt.text);
			fprintf(fc.out, "      actual: %s\n", rhs_fmt.text);
		}
		att_context_failure_append_format("    expected: %s\n", lhs_fmt.text);
		att_context_failure_append_format("      actual: %s\n", rhs_fmt.text);

		char expr[256];
		att_build_expr(expr, sizeof(expr), lhs_expr, &lhs_fmt, rhs_expr, &rhs_fmt);
		if (!fc.suppress) {
			fprintf(fc.out, "    expr: %s\n", expr);
		}
		att_context_failure_append_format("    expr: %s\n", expr);
	}

	if (fatal) {
		att_context_abort();
	}
}

void att_handle_memory(const char *assertion, const char *file, int line, bool fatal, const void *lhs, const void *rhs, size_t size, const char *lhs_expr, const char *rhs_expr)
{
	bool passed = false;
	size_t mismatch_index = SIZE_MAX;
	unsigned char lhs_byte = 0;
	unsigned char rhs_byte = 0;

	if (size == 0) {
		passed = true;
	} else if (!lhs || !rhs) {
		passed = false;
	} else {
		passed = memcmp(lhs, rhs, size) == 0;
		if (!passed) {
			const unsigned char *lbytes = lhs;
			const unsigned char *rbytes = rhs;
			for (size_t i = 0; i < size; ++i) {
				if (lbytes[i] != rbytes[i]) {
					mismatch_index = i;
					lhs_byte = lbytes[i];
					rhs_byte = rbytes[i];
					break;
				}
			}
		}
	}

	att_context_record_assert(fatal, passed);
	if (passed) {
		return;
	}

	char expected_buf[64];
	snprintf(expected_buf, sizeof(expected_buf), "memory equal (len=%zu)", size);
	char actual_buf[128];
	if (size > 0 && lhs && rhs && mismatch_index != SIZE_MAX) {
		snprintf(actual_buf, sizeof(actual_buf), "mismatch at byte %zu (0x%02X vs 0x%02X)", mismatch_index, lhs_byte, rhs_byte);
	} else if (size > 0 && (!lhs || !rhs)) {
		snprintf(actual_buf, sizeof(actual_buf), "%s pointer is NULL", lhs ? "rhs" : "lhs");
	} else {
		snprintf(actual_buf, sizeof(actual_buf), "unexpected mismatch");
	}

	att_formatted lhs_fmt = att_format_pointer(lhs);
	if (lhs_fmt.text == NULL || lhs_fmt.buffer[0] != '\0') {
		lhs_fmt.text = lhs_fmt.buffer;
	}
	att_formatted rhs_fmt = att_format_pointer(rhs);
	if (rhs_fmt.text == NULL || rhs_fmt.buffer[0] != '\0') {
		rhs_fmt.text = rhs_fmt.buffer;
	}
	char expr[256];
	att_build_expr(expr, sizeof(expr), lhs_expr, &lhs_fmt, rhs_expr, &rhs_fmt);

	att_report_failure(fatal, assertion, file, line, expected_buf, actual_buf, expr, NULL, NULL);
	if (fatal) {
		att_context_abort();
	}
}

void att_handle_near(const char *assertion, const char *file, int line, bool fatal, double lhs, double rhs, double epsilon, const char *lhs_expr, const char *rhs_expr, const char *eps_expr)
{
	bool valid = !isnan(lhs) && !isnan(rhs) && !isnan(epsilon) && epsilon >= 0.0;
	bool passed = false;
	double diff = fabs(lhs - rhs);

	if (!valid) {
		passed = false;
	} else if (isinf(lhs) || isinf(rhs)) {
		passed = lhs == rhs;
	} else {
		passed = diff <= epsilon;
	}

	att_context_record_assert(fatal, passed);
	if (passed) {
		return;
	}

	att_formatted lhs_fmt = att_format_double(lhs);
	if (lhs_fmt.text == NULL || lhs_fmt.buffer[0] != '\0') {
		lhs_fmt.text = lhs_fmt.buffer;
	}
	att_formatted rhs_fmt = att_format_double(rhs);
	if (rhs_fmt.text == NULL || rhs_fmt.buffer[0] != '\0') {
		rhs_fmt.text = rhs_fmt.buffer;
	}
	att_formatted eps_fmt = att_format_double(epsilon);
	if (eps_fmt.text == NULL || eps_fmt.buffer[0] != '\0') {
		eps_fmt.text = eps_fmt.buffer;
	}
	char expr[256];
	att_build_expr(expr, sizeof(expr), lhs_expr, &lhs_fmt, rhs_expr, &rhs_fmt);

	char expected_buf[128];
	snprintf(expected_buf, sizeof(expected_buf), "|%s - %s| <= %s", lhs_expr, rhs_expr, eps_expr);

	char actual_buf[128];
	if (!valid) {
		if (epsilon < 0.0) {
			snprintf(actual_buf, sizeof(actual_buf), "epsilon is negative (%s)", eps_fmt.text);
		} else {
			snprintf(actual_buf, sizeof(actual_buf), "comparison undefined (NaN input)");
		}
	} else if (isinf(lhs) || isinf(rhs)) {
		snprintf(actual_buf, sizeof(actual_buf), "infinite values differ");
	} else {
		att_formatted diff_fmt = att_format_double(diff);
		if (diff_fmt.text == NULL || diff_fmt.buffer[0] != '\0') {
			diff_fmt.text = diff_fmt.buffer;
		}
		snprintf(actual_buf, sizeof(actual_buf), "|lhs - rhs| = %s", diff_fmt.text);
	}

	att_report_failure(fatal, assertion, file, line, expected_buf, actual_buf, expr, "epsilon", eps_fmt.text);
	if (fatal) {
		att_context_abort();
	}
}

void att_handle_near_rel(const char *assertion, const char *file, int line, bool fatal, double lhs, double rhs, double rel_eps, const char *lhs_expr, const char *rhs_expr, const char *eps_expr)
{
	bool valid = !isnan(lhs) && !isnan(rhs) && !isnan(rel_eps) && rel_eps >= 0.0;
	bool passed = false;
	double diff = fabs(lhs - rhs);
	double abs_lhs = fabs(lhs);
	double abs_rhs = fabs(rhs);
	double max_abs = abs_lhs > abs_rhs ? abs_lhs : abs_rhs;
	double threshold = rel_eps * max_abs;

	if (!valid) {
		passed = false;
	} else if (isinf(lhs) || isinf(rhs)) {
		passed = lhs == rhs;
	} else if (max_abs < 1e-15) {
		/* Both values near zero: use absolute comparison with rel_eps as absolute threshold */
		passed = diff <= rel_eps;
	} else {
		passed = diff <= threshold;
	}

	att_context_record_assert(fatal, passed);
	if (passed) {
		return;
	}

	att_formatted lhs_fmt = att_format_double(lhs);
	if (lhs_fmt.text == NULL || lhs_fmt.buffer[0] != '\0') {
		lhs_fmt.text = lhs_fmt.buffer;
	}
	att_formatted rhs_fmt = att_format_double(rhs);
	if (rhs_fmt.text == NULL || rhs_fmt.buffer[0] != '\0') {
		rhs_fmt.text = rhs_fmt.buffer;
	}
	att_formatted eps_fmt = att_format_double(rel_eps);
	if (eps_fmt.text == NULL || eps_fmt.buffer[0] != '\0') {
		eps_fmt.text = eps_fmt.buffer;
	}
	char expr[256];
	att_build_expr(expr, sizeof(expr), lhs_expr, &lhs_fmt, rhs_expr, &rhs_fmt);

	char expected_buf[128];
	snprintf(expected_buf, sizeof(expected_buf), "|%s - %s| <= %s * max(|%s|, |%s|)", lhs_expr, rhs_expr, eps_expr, lhs_expr, rhs_expr);

	char actual_buf[128];
	if (!valid) {
		if (rel_eps < 0.0) {
			snprintf(actual_buf, sizeof(actual_buf), "rel_eps is negative (%s)", eps_fmt.text);
		} else {
			snprintf(actual_buf, sizeof(actual_buf), "comparison undefined (NaN input)");
		}
	} else if (isinf(lhs) || isinf(rhs)) {
		snprintf(actual_buf, sizeof(actual_buf), "infinite values differ");
	} else {
		att_formatted diff_fmt = att_format_double(diff);
		if (diff_fmt.text == NULL || diff_fmt.buffer[0] != '\0') {
			diff_fmt.text = diff_fmt.buffer;
		}
		att_formatted threshold_fmt = att_format_double(threshold);
		if (threshold_fmt.text == NULL || threshold_fmt.buffer[0] != '\0') {
			threshold_fmt.text = threshold_fmt.buffer;
		}
		snprintf(actual_buf, sizeof(actual_buf), "|lhs - rhs| = %s, threshold = %s", diff_fmt.text, threshold_fmt.text);
	}

	att_report_failure(fatal, assertion, file, line, expected_buf, actual_buf, expr, "rel_eps", eps_fmt.text);
	if (fatal) {
		att_context_abort();
	}
}

void att_handle_ulp_eq(double a, double b, int64_t max_ulp, const char *file, int line, const char *expr_a, const char *expr_b, const char *expr_ulp, bool fatal)
{
	bool valid = !isnan(a) && !isnan(b) && max_ulp >= 0;
	bool passed = false;
	int64_t ulp_distance = 0;

	if (!valid) {
		passed = false;
	} else if (isinf(a) || isinf(b)) {
		/* Both infinity: must match sign exactly */
		passed = (a == b);
	} else {
		/* Convert doubles to 64-bit integers via union (C strict aliasing safe) */
		union {
			double d;
			uint64_t u;
		} ua, ub;
		ua.d = a;
		ub.d = b;

		/* IEEE 754: sign bit is MSB. For negative numbers, we need two's complement
		   interpretation from 0x8000000000000000 to make ULP counting work correctly */
		uint64_t bits_a = ua.u;
		uint64_t bits_b = ub.u;

		/* If sign bit is set (negative), convert to two's complement from 0x8000000000000000 */
		if (bits_a & 0x8000000000000000ULL) {
			bits_a = 0x8000000000000000ULL - bits_a;
		}
		if (bits_b & 0x8000000000000000ULL) {
			bits_b = 0x8000000000000000ULL - bits_b;
		}

		/* Calculate ULP distance */
		if (bits_a > bits_b) {
			ulp_distance = (int64_t)(bits_a - bits_b);
		} else {
			ulp_distance = (int64_t)(bits_b - bits_a);
		}

		passed = (ulp_distance <= max_ulp);
	}

	att_context_record_assert(fatal, passed);
	if (passed) {
		return;
	}

	att_formatted a_fmt = att_format_double(a);
	if (a_fmt.text == NULL || a_fmt.buffer[0] != '\0') {
		a_fmt.text = a_fmt.buffer;
	}
	att_formatted b_fmt = att_format_double(b);
	if (b_fmt.text == NULL || b_fmt.buffer[0] != '\0') {
		b_fmt.text = b_fmt.buffer;
	}
	char expr[256];
	att_build_expr(expr, sizeof(expr), expr_a, &a_fmt, expr_b, &b_fmt);

	char expected_buf[128];
	snprintf(expected_buf, sizeof(expected_buf), "ULP distance <= %s", expr_ulp);

	char actual_buf[128];
	if (!valid) {
		if (max_ulp < 0) {
			snprintf(actual_buf, sizeof(actual_buf), "max_ulp is negative (%" PRId64 ")", max_ulp);
		} else {
			snprintf(actual_buf, sizeof(actual_buf), "comparison undefined (NaN input)");
		}
	} else if (isinf(a) || isinf(b)) {
		snprintf(actual_buf, sizeof(actual_buf), "infinite values differ");
	} else {
		snprintf(actual_buf, sizeof(actual_buf), "ULP distance = %" PRId64, ulp_distance);
	}

	char ulp_str[32];
	snprintf(ulp_str, sizeof(ulp_str), "%" PRId64, max_ulp);

	const char *assertion_name = fatal ? "ASSERT_ULP_EQ" : "EXPECT_ULP_EQ";
	char assertion_full[128];
	snprintf(assertion_full, sizeof(assertion_full), "%s(%s, %s, %s)", assertion_name, expr_a, expr_b, expr_ulp);

	att_report_failure(fatal, assertion_full, file, line, expected_buf, actual_buf, expr, "max_ulp", ulp_str);
	if (fatal) {
		att_context_abort();
	}
}

static const char *att_status_name(att_status status)
{
	switch (status) {
	case ATT_STATUS_OK:
		return "ok";
	case ATT_STATUS_FAIL:
		return "fail";
	case ATT_STATUS_ABORTED:
		return "aborted";
	default:
		return "unknown";
	}
}

bool att_handle_subtest_expect(const char *assertion, const char *file, int line, const char *name_expr,
	const char *name_value, int min, int max, att_status status, const att_result *result)
{
	int failures = result ? result->failed : 0;
	int fatal_failures = result ? result->fatal_failures : 0;
	int nonfatal_failures = result ? result->nonfatal_failures : 0;
	bool aborted = (status == ATT_STATUS_ABORTED) || (result && result->status == ATT_STATUS_ABORTED);
	bool in_range = (failures >= min) && (failures <= max);
	bool passed = in_range && !aborted;

	att_context_record_assert(false, passed);
	if (passed) {
		return true;
	}

	char expected_buf[64];
	snprintf(expected_buf, sizeof(expected_buf), "failures in [%d,%d]", min, max);

	char actual_buf[128];
	snprintf(actual_buf, sizeof(actual_buf), "%d failures (status=%s)", failures, att_status_name(status));

	char expr_buf[160];
	snprintf(expr_buf, sizeof(expr_buf), "failed=%d, fatal=%d, nonfatal=%d", failures, fatal_failures, nonfatal_failures);

	const char *sub_name = name_value ? name_value : "(null)";
	const char *name_desc = name_expr ? name_expr : "name";

	att_report_failure(false, assertion, file, line, expected_buf, actual_buf, expr_buf, name_desc, sub_name);
	return false;
}

static void att_prepare_subtest_result(const att_test_result *in, att_result *out, att_status status)
{
	if (!out) {
		return;
	}
	out->total = in ? in->assertions_total : 0;
	out->failed = in ? (in->fail_fatal + in->fail_nonfatal) : 0;
	out->fatal_failures = in ? in->fail_fatal : 0;
	out->nonfatal_failures = in ? in->fail_nonfatal : 0;
	out->skipped = (in && in->skipped) ? 1 : 0;
	out->status = status;
}

struct att_subtest_scope {
	att_context_state state;
	att_context_state *previous;
	att_test_case temp;
	char *fullname;
	bool active;
};

static att_subtest_scope *att_subtest_scope_allocate(void)
{
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__APPLE__) && !defined(_MSC_VER)
	/* C11 aligned_alloc for better alignment on Linux ARM64 */
	size_t size = sizeof(att_subtest_scope);
	size_t aligned_size = (size + 15) & ~15;
	att_subtest_scope *scope = aligned_alloc(16, aligned_size);
	if (scope) {
		memset(scope, 0, aligned_size);
	}
#elif defined(_MSC_VER)
	/* MSVC: Use _aligned_malloc */
	att_subtest_scope *scope = _aligned_malloc(sizeof(*scope), 16);
	if (scope) {
		memset(scope, 0, sizeof(*scope));
	}
#else
	att_subtest_scope *scope = malloc(sizeof(*scope));
	if (scope) {
		memset(scope, 0, sizeof(*scope));
	}
#endif
	return scope;
}

att_subtest_scope *att_subtest_scope_enter(const char *name)
{
	att_subtest_scope *scope = att_subtest_scope_allocate();
	if (!scope) {
		return NULL;
	}

	/* Ensure g_ctx is initialized before saving parent context */
	att_context_state *parent = att_get_context();
	scope->previous = parent;

	const char *parent_suite = (parent && parent->active && parent->test && parent->test->suite) ? parent->test->suite : "<subtest>";
	const char *parent_name = (parent && parent->active && parent->test && parent->test->fullname) ? parent->test->fullname : "(test)";
	const char *sub_name = name ? name : "subtest";

	size_t parent_len = strlen(parent_name);
	size_t sub_len = strlen(sub_name);
	size_t total_len = parent_len + 3 + sub_len;

	char *full_name = malloc(total_len + 1);
	if (full_name) {
		snprintf(full_name, total_len + 1, "%s / %s", parent_name, sub_name);
	}
	scope->fullname = full_name;

	scope->temp.suite = parent_suite;
	scope->temp.name = sub_name;
	scope->temp.fullname = full_name ? full_name : sub_name;
	scope->temp.file = (parent && parent->active && parent->test && parent->test->file) ? parent->test->file : "";
	scope->temp.line = (parent && parent->active && parent->test) ? parent->test->line : 0;
	scope->temp.fn = NULL;

	bool color = att_context_color_enabled();
	att_output_format format = (scope->previous && scope->previous->active) ? scope->previous->format : ATT_OUTPUT_DEFAULT;

	/* Initialize scope->state directly without switching g_ctx first */
	memset(&scope->state, 0, sizeof(scope->state));
	scope->state.test = &scope->temp;
	scope->state.color_enabled = color;
	scope->state.format = format;
	scope->state.active = true;
	scope->state.phase = ATT_CONTEXT_PHASE_TEST;
	scope->state.previous = scope->previous;

	/* Now switch g_ctx to the new subtest context */
	g_ctx = &scope->state;
	scope->active = true;
	return scope;
}

/* Helper for att_subtest_scope_protect macro */
int att__subtest_scope_active(const att_subtest_scope *scope)
{
	return (scope && scope->active) ? 1 : 0;
}

/* Helper for att_subtest_scope_protect macro - returns pointer to g_ctx->abort_env
 * This allows the macro to call setjmp directly in the caller's stack frame */
att_jmp_buf *att__get_abort_env_ptr(void)
{
	if (!g_ctx) {
		att_get_context();
	}
	return &g_ctx->abort_env;
}

static att_status att_subtest_scope_finalize(att_subtest_scope *scope, att_result *out)
{
	att_context_state *parent = scope ? scope->previous : att_get_context();
	if (!scope || !scope->active) {
		if (out) {
			memset(out, 0, sizeof(*out));
			out->status = ATT_STATUS_ABORTED;
		}
		g_ctx = parent;
		return ATT_STATUS_ABORTED;
	}

	att_test_result sub_result;
	att_context_end(&sub_result);

	att_status status;
	if (sub_result.skipped) {
		status = ATT_STATUS_OK;
	} else if (sub_result.aborted) {
		status = ATT_STATUS_ABORTED;
	} else if ((sub_result.fail_fatal + sub_result.fail_nonfatal) > 0) {
		status = ATT_STATUS_FAIL;
	} else {
		status = ATT_STATUS_OK;
	}

	att_prepare_subtest_result(&sub_result, out, status);
	if (sub_result.skip_reason) {
		free(sub_result.skip_reason);
	}
	scope->active = false;
	g_ctx = parent;
	return status;
}

att_status att_subtest_scope_leave(att_subtest_scope *scope, att_result *out)
{
	att_status status = ATT_STATUS_ABORTED;
	if (scope) {
		status = att_subtest_scope_finalize(scope, out);
		if (scope->fullname) {
			free(scope->fullname);
		}
#ifdef _MSC_VER
		_aligned_free(scope);
#else
		free(scope);
#endif
	} else {
		if (out) {
			memset(out, 0, sizeof(*out));
			out->status = ATT_STATUS_ABORTED;
		}
	}
	return status;
}

att_status att_run_subtest(const char *name, void (*fn)(void *), void *user, att_result *out)
{
	if (!fn) {
		if (out) {
			memset(out, 0, sizeof(*out));
			out->status = ATT_STATUS_FAIL;
		}
		return ATT_STATUS_FAIL;
	}

	att_subtest_scope *scope = att_subtest_scope_enter(name);
	// Inline setjmp to avoid stack frame issues - setjmp MUST be in this stack frame
	if (scope && scope->active && att_setjmp(g_ctx->abort_env) == 0) {
		fn(user);
	}
	return att_subtest_scope_leave(scope, out);
}

void att_skip(const char *reason)
{
	att_context_skip(reason);
}

void att_replay_captured(const att_captured *captured)
{
	if (!captured || !captured->data || captured->size == 0) {
		return;
	}
	fwrite(captured->data, 1, captured->size, stderr);
	if (captured->data[captured->size - 1] != '\n') {
		fputc('\n', stderr);
	}
}
