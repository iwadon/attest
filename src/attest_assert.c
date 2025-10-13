#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <signal.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "attest/attest.h"
#include "internal/attest_context.h"
#include "internal/attest_internal.h"

typedef struct att_context_state {
	const att_test_case *test;
	bool active;
	bool color_enabled;
	att_output_format format;
	sigjmp_buf abort_env;
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
} att_context_state;

static att_context_state g_ctx_root;
static att_context_state *g_ctx = &g_ctx_root;

static char *att_dup_string(const char *text);
static const char *att_color_fail(void);
static const char *att_color_reset(void);
static bool att_context_failure_append_format(const char *fmt, ...);
static bool g_timeout_handler_installed;

static void att_timeout_signal_handler(int signo)
{
	(void)signo;
	if (!g_ctx || !g_ctx->active) {
		return;
	}
	g_ctx->timeout_triggered = true;
	siglongjmp(g_ctx->abort_env, 1);
}

void att_context_begin(const att_test_case *test, bool color_enabled, att_output_format format)
{
	memset(g_ctx, 0, sizeof(*g_ctx));
	g_ctx->test = test;
	g_ctx->color_enabled = color_enabled;
	g_ctx->format = format;
	g_ctx->active = true;
	g_ctx->phase = ATT_CONTEXT_PHASE_TEST;
}

int att_context_protect(void)
{
	return sigsetjmp(g_ctx->abort_env, 1);
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
	if (!g_ctx->active) {
		return;
	}
	g_ctx->result.aborted = true;
	att_context_fixture_on_abort();
	att_context_timeout_stop();
	siglongjmp(g_ctx->abort_env, 1);
}

bool att_context_color_enabled(void)
{
	return g_ctx->color_enabled;
}

const att_test_case *att_context_current_test(void)
{
	return g_ctx->test;
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
	return g_ctx->phase;
}

att_output_format att_context_get_format(void)
{
	return g_ctx->format;
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
	if (!g_ctx->active) {
		return;
	}

	if (g_ctx->result.skip_reason) {
		free(g_ctx->result.skip_reason);
		g_ctx->result.skip_reason = NULL;
	}

	if (reason) {
		g_ctx->result.skip_reason = att_dup_string(reason);
	}

	g_ctx->result.skipped = true;
	g_ctx->result.aborted = false;

	const att_test_case *test = att_context_current_test();
	const char *test_name = test ? test->fullname : "<unknown>";

	bool suppress_default_output = (g_ctx->format == ATT_OUTPUT_TAP || g_ctx->format == ATT_OUTPUT_JUNIT);
	if (!suppress_default_output) {
		printf("[  SKIPPED ] %s\n", test_name);
		printf("  reason: %s\n", reason ? reason : "(none)");
	}

	att_context_fixture_on_abort();
	siglongjmp(g_ctx->abort_env, 1);
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
static const char *att_color_fail(void)
{
	return att_context_color_enabled() ? "\033[31m" : "";
}

static const char *att_color_file(void)
{
	return att_context_color_enabled() ? "\033[90m" : "";
}

static const char *att_color_reset(void)
{
	return att_context_color_enabled() ? "\033[0m" : "";
}

typedef struct att_formatted {
	char buffer[128];
	const char *text;
} att_formatted;

static att_formatted att_format_signed(long long value)
{
	att_formatted fmt;
	snprintf(fmt.buffer, sizeof(fmt.buffer), "%lld", value);
	fmt.text = fmt.buffer;
	return fmt;
}

static att_formatted att_format_unsigned(unsigned long long value)
{
	att_formatted fmt;
	snprintf(fmt.buffer, sizeof(fmt.buffer), "%llu", value);
	fmt.text = fmt.buffer;
	return fmt;
}

static att_formatted att_format_double(double value)
{
	att_formatted fmt;
	snprintf(fmt.buffer, sizeof(fmt.buffer), "%.9g", value);
	fmt.text = fmt.buffer;
	return fmt;
}

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

static void att_report_failure(bool fatal, const char *assertion, const char *file, int line, const char *expected, const char *actual, const char *expr_detail, const char *extra_label, const char *extra_value)
{
	const att_test_case *test = att_context_current_test();
	const char *test_name = test ? test->fullname : "<unknown>";
	const char *fail_color = att_color_fail();
	const char *file_color = att_color_file();
	const char *reset = att_color_reset();
	const char *phase = att_phase_tag(att_context_phase_current());
	FILE *out = stderr;

	bool suppress_default_output = (g_ctx && g_ctx->active && (g_ctx->format == ATT_OUTPUT_TAP || g_ctx->format == ATT_OUTPUT_JUNIT));

	if (!suppress_default_output) {
		fprintf(out, "%s[  FAILED  ]%s %s", fail_color, reset, test_name);
		if (phase) {
			fprintf(out, " (%s)", phase);
		}
		fprintf(out, "\n");
	}
	if (phase) {
		att_context_failure_append_format("[  FAILED  ] %s (%s)\n", test_name, phase);
	} else {
		att_context_failure_append_format("[  FAILED  ] %s\n", test_name);
	}

	for (int i = 0; i < g_ctx->info_stack_size; ++i) {
		att_context_failure_append_format("  context: %s\n", g_ctx->info_stack[i]);
	}
	if (!suppress_default_output) {
		for (int i = 0; i < g_ctx->info_stack_size; ++i) {
			fprintf(out, "  context: %s\n", g_ctx->info_stack[i]);
		}
		fprintf(out, "%s  ", file_color);
		if (phase) {
			fprintf(out, "(%s) ", phase);
		}
		fprintf(out, "%s:%d: %s failed%s%s\n",
			file,
			line,
			assertion,
			fatal ? " (fatal)." : ".",
			reset);
	}
	if (phase) {
		att_context_failure_append_format("  (%s) %s:%d: %s failed%s\n",
			phase,
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
	if (!suppress_default_output) {
		fprintf(out, "    expected: %s\n", expected);
	}
	att_context_failure_append_format("    expected: %s\n", expected);
	if (!suppress_default_output) {
		fprintf(out, "      actual: %s\n", actual);
	}
	att_context_failure_append_format("      actual: %s\n", actual);
	if (extra_label && extra_value) {
		if (!suppress_default_output) {
			fprintf(out, "    %s: %s\n", extra_label, extra_value);
		}
		att_context_failure_append_format("    %s: %s\n", extra_label, extra_value);
	}
	if (!suppress_default_output) {
		fprintf(out, "    expr: %s\n", expr_detail);
	}
	att_context_failure_append_format("    expr: %s\n", expr_detail);
}

static bool att_compare_values(int op, long long lhs, long long rhs)
{
	switch (op) {
	case ATT_COMP_EQ:
		return lhs == rhs;
	case ATT_COMP_NE:
		return lhs != rhs;
	case ATT_COMP_LT:
		return lhs < rhs;
	case ATT_COMP_LE:
		return lhs <= rhs;
	case ATT_COMP_GT:
		return lhs > rhs;
	case ATT_COMP_GE:
		return lhs >= rhs;
	default:
		return false;
	}
}

static bool att_compare_unsigned_values(int op, unsigned long long lhs, unsigned long long rhs)
{
	switch (op) {
	case ATT_COMP_EQ:
		return lhs == rhs;
	case ATT_COMP_NE:
		return lhs != rhs;
	case ATT_COMP_LT:
		return lhs < rhs;
	case ATT_COMP_LE:
		return lhs <= rhs;
	case ATT_COMP_GT:
		return lhs > rhs;
	case ATT_COMP_GE:
		return lhs >= rhs;
	default:
		return false;
	}
}

void att_handle_compare_signed(int op, const char *assertion, const char *file, int line, bool fatal, const char *lhs_expr, const char *rhs_expr, long long lhs, long long rhs)
{
	bool passed = att_compare_values(op, lhs, rhs);
	att_context_record_assert(fatal, passed);
	if (passed) {
		return;
	}

	att_formatted lhs_fmt = att_format_signed(lhs);
	att_formatted rhs_fmt = att_format_signed(rhs);
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
	att_formatted rhs_fmt = att_format_unsigned(rhs);
	char expr[256];
	att_build_expr(expr, sizeof(expr), lhs_expr, &lhs_fmt, rhs_expr, &rhs_fmt);
	att_report_failure(fatal, assertion, file, line, lhs_fmt.text, rhs_fmt.text, expr, NULL, NULL);
	if (fatal) {
		att_context_abort();
	}
}

static bool att_compare_double_values(int op, double lhs, double rhs)
{
	switch (op) {
	case ATT_COMP_EQ:
		return lhs == rhs;
	case ATT_COMP_NE:
		return lhs != rhs;
	case ATT_COMP_LT:
		return lhs < rhs;
	case ATT_COMP_LE:
		return lhs <= rhs;
	case ATT_COMP_GT:
		return lhs > rhs;
	case ATT_COMP_GE:
		return lhs >= rhs;
	default:
		return false;
	}
}

void att_handle_compare_double(int op, const char *assertion, const char *file, int line, bool fatal, const char *lhs_expr, const char *rhs_expr, double lhs, double rhs)
{
	bool passed = att_compare_double_values(op, lhs, rhs);
	att_context_record_assert(fatal, passed);
	if (passed) {
		return;
	}

	att_formatted lhs_fmt = att_format_double(lhs);
	att_formatted rhs_fmt = att_format_double(rhs);
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
	att_formatted rhs_fmt = att_format_pointer(rhs);
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

void att_handle_string(int op, const char *assertion, const char *file, int line, bool fatal, const char *lhs_expr, const char *rhs_expr, const char *lhs, const char *rhs)
{
	bool eq = att_strings_equal(lhs, rhs);
	bool passed = (op == ATT_COMP_EQ) ? eq : !eq;

	att_context_record_assert(fatal, passed);
	if (passed) {
		return;
	}

	att_formatted lhs_fmt = att_format_string(lhs);
	att_formatted rhs_fmt = att_format_string(rhs);
	char expr[256];
	att_build_expr(expr, sizeof(expr), lhs_expr, &lhs_fmt, rhs_expr, &rhs_fmt);
	att_report_failure(fatal, assertion, file, line, lhs_fmt.text, rhs_fmt.text, expr, NULL, NULL);
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
	att_formatted rhs_fmt = att_format_pointer(rhs);
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
	att_formatted rhs_fmt = att_format_double(rhs);
	att_formatted eps_fmt = att_format_double(epsilon);
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
		snprintf(actual_buf, sizeof(actual_buf), "|lhs - rhs| = %s", diff_fmt.text);
	}

	att_report_failure(fatal, assertion, file, line, expected_buf, actual_buf, expr, "epsilon", eps_fmt.text);
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
	att_subtest_scope *scope = malloc(sizeof(*scope));
	if (scope) {
		memset(scope, 0, sizeof(*scope));
	}
	return scope;
}

att_subtest_scope *att_subtest_scope_enter(const char *name)
{
	att_subtest_scope *scope = att_subtest_scope_allocate();
	if (!scope) {
		return NULL;
	}

	att_context_state *parent = g_ctx;
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
	g_ctx = &scope->state;
	att_context_begin(&scope->temp, color, format);
	g_ctx->previous = scope->previous;
	scope->active = true;
	return scope;
}

int att_subtest_scope_protect(att_subtest_scope *scope)
{
	if (!scope || !scope->active) {
		return 1;
	}
	return att_context_protect();
}

static att_status att_subtest_scope_finalize(att_subtest_scope *scope, att_result *out)
{
	att_context_state *parent = scope ? scope->previous : g_ctx;
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
		free(scope);
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
	if (att_subtest_scope_protect(scope) == 0) {
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
