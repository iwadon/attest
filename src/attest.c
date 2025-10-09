#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attest/attest.h"
#include "internal/attest_context.h"
#include "internal/attest_internal.h"

static const char* att_color_green(bool enabled)
{
	return enabled ? "\033[32m" : "";
}

static const char* att_color_red(bool enabled)
{
	return enabled ? "\033[31m" : "";
}

static const char* att_color_reset(bool enabled)
{
	return enabled ? "\033[0m" : "";
}

static int att_collect_suites(const att_registry* registry, const att_cli_options* opts, const char*** suites, size_t* suite_count)
{
	const char** names = NULL;
	size_t count = 0;
	size_t capacity = 0;

	for (size_t i = 0; i < registry->count; ++i) {
		const att_test_case* test = &registry->tests[i];
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
			const char** grown = realloc(names, new_capacity * sizeof(*grown));
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

int att_run_tests(const att_registry* registry, const att_cli_options* opts, att_summary* summary)
{
	if (!registry || !opts || !summary) {
		fprintf(stderr, "error: initialization failed\n");
		return 3;
	}

	memset(summary, 0, sizeof(*summary));
	summary->tests_total = (int)registry->count;

	size_t suite_count = 0;
	const char** suite_names = NULL;
	if (att_collect_suites(registry, opts, &suite_names, &suite_count) != 0) {
		return 3;
	}
	summary->suites_total = (int)suite_count;

	for (size_t i = 0; i < registry->count; ++i) {
		const att_test_case* test = &registry->tests[i];
		if (!att_filter_match(test, opts)) {
			continue;
		}
		++summary->tests_selected;
	}

	printf("[==========] Running %d tests from %d suites.\n", summary->tests_selected, summary->suites_total);

	for (size_t i = 0; i < registry->count; ++i) {
		const att_test_case* test = &registry->tests[i];
		if (!att_filter_match(test, opts)) {
			continue;
		}
		att_context_begin(test, opts->color_enabled);
		if (att_context_protect() == 0) {
			test->fn();
		}
		att_test_result result;
		att_context_end(&result);
		++summary->tests_run;
		summary->assertions_total += result.assertions_total;
		int test_failures = result.fail_fatal + result.fail_nonfatal;
		summary->failures_total += test_failures;
		if (test_failures > 0 || result.aborted) {
			++summary->tests_failed;
		}
	}

	free(suite_names);

	return summary->tests_failed > 0 ? 1 : 0;
}

void att_report_summary(const att_summary* summary, bool color_enabled)
{
	if (!summary) {
		return;
	}

	const char* color_fail = att_color_red(color_enabled);
	const char* color_pass = att_color_green(color_enabled);
	const char* color_reset = att_color_reset(color_enabled);

	printf("%s[==========]%s %d tests ran. %d failures.\n",
	    summary->tests_failed ? color_fail : color_pass,
	    color_reset,
	    summary->tests_run,
	    summary->failures_total);

	int passed = summary->tests_run - summary->tests_failed;
	if (summary->tests_failed > 0) {
		printf("%s[  FAILED  ]%s %d tests.\n", color_fail, color_reset, summary->tests_failed);
		if (passed > 0) {
			printf("%s[  PASSED  ]%s %d tests.\n", color_pass, color_reset, passed);
		}
	} else {
		printf("%s[  PASSED  ]%s %d tests.\n", color_pass, color_reset, passed);
	}
}

int attest_main(int argc, char** argv)
{
	att_registry* registry = att_registry_get();
	const char* registry_error = att_registry_error();
	if (registry_error) {
		fprintf(stderr, "%s\n", registry_error);
		return 3;
	}

	att_cli_options opts;
	char* cli_error = NULL;
	int cli_rc = att_cli_parse(argc, argv, &opts, &cli_error);
	if (cli_rc != 0) {
		if (cli_error) {
			fprintf(stderr, "%s\n", cli_error);
			free(cli_error);
		}
		return 2;
	}

	if (opts.list_only) {
		att_print_list(registry, &opts);
		return 0;
	}

	att_summary summary;
	int rc = att_run_tests(registry, &opts, &summary);
	att_report_summary(&summary, opts.color_enabled);
	return rc;
}
