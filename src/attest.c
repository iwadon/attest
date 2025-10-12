#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attest/attest.h"
#include "internal/attest_context.h"
#include "internal/attest_internal.h"

static const char *att_color_green(bool enabled)
{
	return enabled ? "\033[32m" : "";
}

static const char *att_color_red(bool enabled)
{
	return enabled ? "\033[31m" : "";
}

static const char *att_color_reset(bool enabled)
{
	return enabled ? "\033[0m" : "";
}

typedef struct att_recorded_test {
	const att_test_case *test;
	att_test_result result;
} att_recorded_test;

static void att_write_xml_escaped(FILE *out, const char *text)
{
	if (!out || !text) {
		return;
	}
	for (const char *it = text; *it; ++it) {
		switch (*it) {
		case '&':
			fputs("&amp;", out);
			break;
		case '<':
			fputs("&lt;", out);
			break;
		case '>':
			fputs("&gt;", out);
			break;
		case '"':
			fputs("&quot;", out);
			break;
		case '\'':
			fputs("&apos;", out);
			break;
		default:
			fputc(*it, out);
			break;
		}
	}
}

static int att_write_junit_report(const att_cli_options *opts, const att_summary *summary, const att_recorded_test *tests, size_t count)
{
	if (!opts || !summary || !tests) {
		return -1;
	}
	const char *path = opts->output_path ? opts->output_path : "test_detail.xml";
	FILE *out = NULL;
	bool use_stdout = (strcmp(path, "-") == 0);
	if (use_stdout) {
		out = stdout;
	} else {
		out = fopen(path, "w");
		if (!out) {
			fprintf(stderr, "error: failed to open '%s' for writing: %s\n", path, strerror(errno));
			return -1;
		}
	}

	fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", out);
	fprintf(out, "<testsuite name=\"attest\" tests=\"%d\" failures=\"%d\" skipped=\"%d\" time=\"0\">\n",
		summary->tests_run,
		summary->tests_failed,
		summary->tests_skipped);

	for (size_t i = 0; i < count; ++i) {
		const att_recorded_test *record = &tests[i];
		const att_test_case *test = record->test;
		const char *suite = test ? test->suite : "";
		const char *name = test ? test->name : "";
		fprintf(out, "  <testcase classname=\"");
		att_write_xml_escaped(out, suite);
		fprintf(out, "\" name=\"");
		att_write_xml_escaped(out, name);
		fprintf(out, "\" time=\"0\"");
		int failures = record->result.fail_fatal + record->result.fail_nonfatal;
		bool aborted = record->result.aborted;
		if (record->result.skipped) {
			fprintf(out, ">\n    <skipped");
			const char *reason = record->result.skip_reason ? record->result.skip_reason : "";
			if (*reason) {
				fprintf(out, " message=\"");
				att_write_xml_escaped(out, reason);
				fputc('\"', out);
			}
			fprintf(out, "/>\n  </testcase>\n");
			continue;
		}
		if (failures > 0 || aborted) {
			const char *failure_message = record->result.timed_out ? "Timeout" : "Test failed";
			fprintf(out, ">\n    <failure message=\"");
			att_write_xml_escaped(out, failure_message);
			fprintf(out, "\">");
			if (record->result.timed_out) {
				fprintf(out, "timeout after %d ms", record->result.timeout_ms);
			} else {
				const char *log = record->result.failure_log ? record->result.failure_log : "";
				if (*log) {
					att_write_xml_escaped(out, log);
				}
			}
			fprintf(out, "</failure>\n  </testcase>\n");
			continue;
		}
		fprintf(out, "/>\n");
	}

	fputs("</testsuite>\n", out);
	if (!use_stdout) {
		fclose(out);
	}
	return 0;
}

static int att_collect_suites(const att_registry *registry, const att_cli_options *opts, const char ***suites, size_t *suite_count)
{
	const char **names = NULL;
	size_t count = 0;
	size_t capacity = 0;

	for (size_t i = 0; i < registry->count; ++i) {
		const att_test_case *test = &registry->tests[i];
		if (!att_filter_match(test, opts)) {
			continue;
		}
		bool seen = false;
		for (size_t s = 0; s < count; ++s) {
			if (strcmp(names[s], test->suite) == 0) {
				seen = true;
				break;
			}
		}
		if (seen) {
			continue;
		}
		if (count == capacity) {
			size_t new_capacity = capacity ? capacity * 2 : 4;
			const char **grown = realloc(names, new_capacity * sizeof(*grown));
			if (!grown) {
				free(names);
				fprintf(stderr, "error: initialization failed\n");
				return -1;
			}
			names = grown;
			capacity = new_capacity;
		}
		names[count++] = test->suite;
	}

	if (suites) {
		*suites = names;
	} else {
		free(names);
	}
	if (suite_count) {
		*suite_count = count;
	}
	return 0;
}

int att_run_tests(const att_registry *registry, const att_cli_options *opts, att_summary *summary)
{
	if (!registry || !opts || !summary) {
		fprintf(stderr, "error: initialization failed\n");
		return 3;
	}

	memset(summary, 0, sizeof(*summary));
	summary->tests_total = (int)registry->count;
	bool tap_mode = opts->format == ATT_OUTPUT_TAP;
	bool junit_mode = opts->format == ATT_OUTPUT_JUNIT;
	att_recorded_test *records = NULL;
	size_t record_count = 0;

	size_t suite_count = 0;
	const char **suite_names = NULL;
	if (att_collect_suites(registry, opts, &suite_names, &suite_count) != 0) {
		return 3;
	}
	summary->suites_total = (int)suite_count;

	for (size_t i = 0; i < registry->count; ++i) {
		const att_test_case *test = &registry->tests[i];
		if (!att_filter_match(test, opts)) {
			continue;
		}
		++summary->tests_selected;
	}

	if (junit_mode) {
		/* JUnit format suppresses default header. */
	} else if (tap_mode) {
		printf("1..%d\n", summary->tests_selected);
	} else {
		printf("[==========] Running %d tests from %d suites.\n", summary->tests_selected, summary->suites_total);
	}

	if (junit_mode) {
		size_t capacity = summary->tests_selected > 0 ? (size_t)summary->tests_selected : 1;
		records = calloc(capacity, sizeof(*records));
		if (!records) {
			fprintf(stderr, "error: allocation failure\n");
			free((void *)suite_names);
			return 3;
		}
	}

	int tap_index = 0;

	for (size_t i = 0; i < registry->count; ++i) {
		const att_test_case *test = &registry->tests[i];
		if (!att_filter_match(test, opts)) {
			continue;
		}
		if (!test->fn) {
			fprintf(stderr, "error: test '%s' has null function pointer\n", test->fullname);
			continue;
		}
		att_context_begin(test, opts->color_enabled);
		att_context_capture_failures(junit_mode);
		if (opts->timeout_ms > 0) {
			att_context_timeout_start(opts->timeout_ms);
		} else {
			att_context_timeout_stop();
		}
		int protect_rc = att_context_protect();
		if (protect_rc == 0) {
			test->fn();
		}
		att_test_result result;
		att_context_end(&result);
		att_context_timeout_stop();
		++summary->tests_run;
		summary->assertions_total += result.assertions_total;
		++tap_index;

		if (junit_mode) {
			records[record_count].test = test;
			records[record_count].result = result;
			++record_count;
		}

		if (result.skipped) {
			++summary->tests_skipped;
			if (tap_mode) {
				const char *reason = result.skip_reason ? result.skip_reason : "(none)";
				printf("ok %d %s # SKIP %s\n", tap_index, test->fullname, reason);
			}
			if (!junit_mode) {
				if (result.skip_reason) {
					free(result.skip_reason);
				}
				if (result.failure_log) {
					free(result.failure_log);
				}
			}
			continue;
		}
		int test_failures = result.fail_fatal + result.fail_nonfatal;
		summary->failures_total += test_failures;
		if (result.timed_out) {
			++summary->timeouts;
		}
		bool failed = (test_failures > 0) || result.aborted;
		if (tap_mode) {
			printf("%s %d %s", failed ? "not ok" : "ok", tap_index, test->fullname);
			if (failed) {
				printf(" # FAILED");
				if (result.timed_out) {
					printf(" (timeout)");
				}
			}
			printf("\n");
		}
		if (failed) {
			++summary->tests_failed;
		}
		if (!junit_mode) {
			if (result.skip_reason) {
				free(result.skip_reason);
			}
			if (result.failure_log) {
				free(result.failure_log);
			}
		}
	}

	free(suite_names);
	if (junit_mode) {
		int rc = att_write_junit_report(opts, summary, records, record_count);
		for (size_t i = 0; i < record_count; ++i) {
			if (records[i].result.skip_reason) {
				free(records[i].result.skip_reason);
			}
			if (records[i].result.failure_log) {
				free(records[i].result.failure_log);
			}
		}
		free(records);
		if (rc != 0) {
			return 3;
		}
	}

	return summary->tests_failed > 0 ? 1 : 0;
}

void att_report_summary(const att_summary *summary, bool color_enabled)
{
	if (!summary) {
		return;
	}

	const char *color_fail = att_color_red(color_enabled);
	const char *color_pass = att_color_green(color_enabled);
	const char *color_reset = att_color_reset(color_enabled);

	printf("%s[==========]%s %d tests ran. %d failures",
		summary->tests_failed ? color_fail : color_pass,
		color_reset,
		summary->tests_run,
		summary->failures_total);
	if (summary->tests_skipped > 0) {
		printf(", %d skipped", summary->tests_skipped);
	}
	printf(".\n");

	int passed = summary->tests_run - summary->tests_failed - summary->tests_skipped;
	if (passed < 0) {
		passed = 0;
	}
	if (summary->tests_failed > 0) {
		printf("%s[  FAILED  ]%s %d tests.\n", color_fail, color_reset, summary->tests_failed);
		if (passed > 0) {
			printf("%s[  PASSED  ]%s %d tests.\n", color_pass, color_reset, passed);
		}
	} else {
		printf("%s[  PASSED  ]%s %d tests.\n", color_pass, color_reset, passed);
	}
	if (summary->tests_skipped > 0) {
		printf("[  SKIPPED ] %d tests.\n", summary->tests_skipped);
	}
	if (summary->timeouts > 0) {
		printf("[----------] timeouts: %d\n", summary->timeouts);
	}
}

int attest_main(int argc, char **argv)
{
	att_registry *registry = att_registry_get();
	const char *registry_error = att_registry_error();
	if (registry_error) {
		fprintf(stderr, "%s\n", registry_error);
		return 3;
	}

	att_cli_options opts;
	char *cli_error = NULL;
	int cli_rc = att_cli_parse(argc, argv, &opts, &cli_error);
	int exit_code = 0;
	bool registry_locked = false;
	if (cli_rc != 0) {
		if (cli_error) {
			fprintf(stderr, "%s\n", cli_error);
			free(cli_error);
		}
		exit_code = 2;
		goto cleanup;
	}

	att_registry_finalize();
	registry_locked = true;

	if (opts.list_only) {
		att_print_list(registry, &opts);
		exit_code = 0;
		goto cleanup;
	}

	att_summary summary;
	exit_code = att_run_tests(registry, &opts, &summary);
	if (opts.format == ATT_OUTPUT_DEFAULT) {
		att_report_summary(&summary, opts.color_enabled);
	}

cleanup:
	if (registry_locked) {
		registry->frozen = false;
	}
	att_cli_dispose(&opts);
	return exit_code;
}
