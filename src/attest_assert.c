#include <inttypes.h>
#include <math.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attest/attest.h"
#include "internal/attest_context.h"
#include "internal/attest_internal.h"

typedef struct att_context_state {
	const att_test_case *test;
	bool active;
	bool color_enabled;
	jmp_buf abort_env;
	att_test_result result;
} att_context_state;

static att_context_state g_ctx;

void att_context_begin(const att_test_case *test, bool color_enabled)
{
	memset(&g_ctx, 0, sizeof(g_ctx));
	g_ctx.test = test;
	g_ctx.color_enabled = color_enabled;
	g_ctx.active = true;
}

int att_context_protect(void)
{
	return setjmp(g_ctx.abort_env);
}

void att_context_end(att_test_result *out_result)
{
	if (out_result) {
		*out_result = g_ctx.result;
	}
	g_ctx.active = false;
}

void att_context_record_assert(bool fatal, bool passed)
{
	if (!g_ctx.active) {
		return;
	}
	++g_ctx.result.assertions_total;
	if (!passed) {
		att_context_register_failure(fatal);
	}
}

void att_context_register_failure(bool fatal)
{
	if (!g_ctx.active) {
		return;
	}
	if (fatal) {
		++g_ctx.result.fail_fatal;
	} else {
		++g_ctx.result.fail_nonfatal;
	}
}

void att_context_abort(void)
{
	if (!g_ctx.active) {
		return;
	}
	g_ctx.result.aborted = true;
	longjmp(g_ctx.abort_env, 1);
}

bool att_context_color_enabled(void)
{
	return g_ctx.color_enabled;
}

const att_test_case *att_context_current_test(void)
{
	return g_ctx.test;
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

static void att_report_failure(bool fatal, const char *assertion, const char *file, int line, const char *expected, const char *actual, const char *expr_detail, const char *extra_label, const char *extra_value)
{
	const att_test_case *test = att_context_current_test();
	const char *test_name = test ? test->fullname : "<unknown>";
	const char *fail_color = att_color_fail();
	const char *file_color = att_color_file();
	const char *reset = att_color_reset();
	FILE *out = stderr;

	fprintf(out, "%s[  FAILED  ]%s %s\n", fail_color, reset, test_name);
	fprintf(out, "%s  %s:%d: %s failed%s%s\n",
		file_color,
		file,
		line,
		assertion,
		fatal ? " (fatal)." : ".",
		reset);
	fprintf(out, "    expected: %s\n", expected);
	fprintf(out, "      actual: %s\n", actual);
	if (extra_label && extra_value) {
		fprintf(out, "    %s: %s\n", extra_label, extra_value);
	}
	fprintf(out, "    expr: %s\n", expr_detail);
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
	out->skipped = 0;
	out->status = status;
}

struct att_subtest_scope {
	att_context_state saved;
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

	scope->saved = g_ctx;

	const char *parent_suite = (g_ctx.active && g_ctx.test && g_ctx.test->suite) ? g_ctx.test->suite : "<subtest>";
	const char *parent_name = (g_ctx.active && g_ctx.test && g_ctx.test->fullname) ? g_ctx.test->fullname : "(test)";
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
	scope->temp.file = (g_ctx.active && g_ctx.test && g_ctx.test->file) ? g_ctx.test->file : "";
	scope->temp.line = (g_ctx.active && g_ctx.test) ? g_ctx.test->line : 0;
	scope->temp.fn = NULL;

	bool color = att_context_color_enabled();
	att_context_begin(&scope->temp, color);
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
	if (!scope || !scope->active) {
		if (out) {
			memset(out, 0, sizeof(*out));
			out->status = ATT_STATUS_ABORTED;
		}
		return ATT_STATUS_ABORTED;
	}

	att_test_result sub_result;
	att_context_end(&sub_result);

	att_status status;
	if (sub_result.aborted) {
		status = ATT_STATUS_ABORTED;
	} else if ((sub_result.fail_fatal + sub_result.fail_nonfatal) > 0) {
		status = ATT_STATUS_FAIL;
	} else {
		status = ATT_STATUS_OK;
	}

	att_prepare_subtest_result(&sub_result, out, status);
	scope->active = false;
	g_ctx = scope->saved;
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
