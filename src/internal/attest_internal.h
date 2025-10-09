#ifndef ATTEST_INTERNAL_H
#define ATTEST_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "attest/attest.h"

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
} att_registry;

typedef struct att_cli_options {
	bool list_only;
	bool color_enabled;
	const char* filter_raw;
	char** filters;
	size_t filter_count;
} att_cli_options;

typedef struct att_summary {
	int suites_total;
	int tests_total;
	int tests_selected;
	int tests_run;
	int tests_failed;
	int assertions_total;
	int failures_total;
} att_summary;

att_registry* att_registry_get(void);
void att_registry_finalize(void);
int att_registry_add(const char* suite, const char* name, att_test_fn fn, const char* file, int line, const char** error);
const char* att_registry_error(void);

int att_cli_parse(int argc, char** argv, att_cli_options* out_opts, char** err_msg);
bool att_filter_match(const att_test_case* test, const att_cli_options* opts);
void att_print_list(const att_registry* registry, const att_cli_options* opts);
int att_run_tests(const att_registry* registry, const att_cli_options* opts, att_summary* summary);
void att_report_summary(const att_summary* summary, bool color_enabled);

#endif /* ATTEST_INTERNAL_H */
