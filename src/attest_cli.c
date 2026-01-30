#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_SC_NPROCESSORS_ONLN)
#include <unistd.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#endif

#include "attest/attest.h"
#include "internal/attest_internal.h"

static char *att_strdup(const char *source)
{
	if (!source) {
		return NULL;
	}
	size_t len = strlen(source);
	char *copy = malloc(len + 1);
	if (!copy) {
		return NULL;
	}
	memcpy(copy, source, len + 1);
	return copy;
}

static char *att_strndup(const char *source, size_t length)
{
	char *copy = malloc(length + 1);
	if (!copy) {
		return NULL;
	}
	if (length) {
		memcpy(copy, source, length);
	}
	copy[length] = '\0';
	return copy;
}

static char *att_format_unknown_option(const char *option)
{
	static const char prefix[] = "error: unknown option '";
	size_t opt_len = strlen(option);
	char *buffer = malloc(sizeof(prefix) - 1 + opt_len + 2);
	if (!buffer) {
		return NULL;
	}
	memcpy(buffer, prefix, sizeof(prefix) - 1);
	memcpy(buffer + sizeof(prefix) - 1, option, opt_len);
	buffer[sizeof(prefix) - 1 + opt_len] = '\'';
	buffer[sizeof(prefix) - 1 + opt_len + 1] = '\0';
	return buffer;
}

static int att_get_cpu_count(void)
{
#if defined(_SC_NPROCESSORS_ONLN)
	long cpus = sysconf(_SC_NPROCESSORS_ONLN);
	return (cpus > 0) ? (int)cpus : 1;
#elif defined(_WIN32)
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return (int)sysinfo.dwNumberOfProcessors;
#else
	return 1;
#endif
}

static char *att_normalize_pattern(const char *pattern)
{
	if (!pattern || !*pattern) {
		return att_strdup("*");
	}

	if (pattern[0] == '.') {
		size_t len = strlen(pattern);
		char *normalized = malloc(len + 2);
		if (!normalized) {
			return NULL;
		}
		normalized[0] = '*';
		memcpy(normalized + 1, pattern, len + 1);
		return normalized;
	}

	if (strchr(pattern, '.') == NULL) {
		size_t len = strlen(pattern);
		char *normalized = malloc(len + 3);
		if (!normalized) {
			return NULL;
		}
		memcpy(normalized, pattern, len);
		normalized[len] = '.';
		normalized[len + 1] = '*';
		normalized[len + 2] = '\0';
		return normalized;
	}

	return att_strdup(pattern);
}

static int att_parse_filter_patterns(const char *raw, att_cli_options *opts, char **err_msg)
{
	if (!raw) {
		return 0;
	}

	char *raw_copy = att_strdup(raw);
	if (!raw_copy) {
		if (err_msg) {
			*err_msg = att_strdup("error: allocation failure");
		}
		return -1;
	}

	size_t segments = 1;
	for (const char *it = raw; *it; ++it) {
		if (*it == ';') {
			++segments;
		}
	}

	char **filters = calloc(segments, sizeof(char *));
	char **negative_filters = calloc(segments, sizeof(char *));
	if (!filters || !negative_filters) {
		free(raw_copy);
		free(filters);
		free(negative_filters);
		if (err_msg) {
			*err_msg = att_strdup("error: allocation failure");
		}
		return -1;
	}

	size_t index = 0;
	size_t neg_index = 0;
	const char *head = raw;
	while (true) {
		const char *delim = strchr(head, ';');
		size_t length = delim ? (size_t)(delim - head) : strlen(head);
		char *slice = att_strndup(head, length);
		if (!slice) {
			if (err_msg) {
				*err_msg = att_strdup("error: allocation failure");
			}
			goto fail;
		}

		/* Check if this is a negative filter (starts with '-') */
		bool is_negative = (slice[0] == '-' && slice[1] != '\0');
		const char *pattern_start = is_negative ? slice + 1 : slice;

		char *normalized = att_normalize_pattern(pattern_start);
		free(slice);
		if (!normalized) {
			if (err_msg) {
				*err_msg = att_strdup("error: allocation failure");
			}
			goto fail;
		}

		if (is_negative) {
			negative_filters[neg_index++] = normalized;
		} else {
			filters[index++] = normalized;
		}

		if (!delim) {
			break;
		}
		head = delim + 1;
	}

	opts->filter_raw = raw_copy;
	opts->filters = filters;
	opts->filter_count = index;
	opts->negative_filters = negative_filters;
	opts->negative_filter_count = neg_index;
	return 0;

fail:
	for (size_t i = 0; i < index; ++i) {
		free(filters[i]);
	}
	for (size_t i = 0; i < neg_index; ++i) {
		free(negative_filters[i]);
	}
	free(filters);
	free(negative_filters);
	free(raw_copy);
	return -1;
}

int att_cli_parse(int argc, char **argv, att_cli_options *out_opts, char **err_msg)
{
	if (!out_opts) {
		if (err_msg) {
			/* Return an allocated error string so callers can free it safely. */
			*err_msg = att_strdup("error: internal");
			if (!*err_msg) {
				*err_msg = att_strdup("error: allocation failure");
			}
		}
		return -1;
	}

	out_opts->list_only = false;
	out_opts->help_requested = false;
	out_opts->color_enabled = true;
	out_opts->shuffle = false;
	out_opts->shuffle_seed = 0;
	out_opts->filter_raw = NULL;
	out_opts->filters = NULL;
	out_opts->filter_count = 0;
	out_opts->negative_filters = NULL;
	out_opts->negative_filter_count = 0;
	out_opts->format = ATT_OUTPUT_DEFAULT;
	out_opts->output_path = NULL;
	out_opts->timeout_ms = 0;
	out_opts->jobs = 1;

	for (int i = 1; i < argc; ++i) {
		const char *arg = argv[i];
		if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
			out_opts->help_requested = true;
			continue;
		}
		if (strncmp(arg, "--filter=", 9) == 0) {
			const char *value = arg + 9;
			if (att_parse_filter_patterns(value, out_opts, err_msg) != 0) {
				return 1;
			}
			continue;
		}
		if (strcmp(arg, "--list") == 0) {
			out_opts->list_only = true;
			continue;
		}
		if (strcmp(arg, "--no-color") == 0) {
			out_opts->color_enabled = false;
			continue;
		}
		if (strncmp(arg, "--format=", 9) == 0) {
			const char *value = arg + 9;
			if (strcmp(value, "default") == 0) {
				out_opts->format = ATT_OUTPUT_DEFAULT;
			} else if (strcmp(value, "tap") == 0) {
				out_opts->format = ATT_OUTPUT_TAP;
			} else if (strcmp(value, "junit") == 0) {
				out_opts->format = ATT_OUTPUT_JUNIT;
			} else {
				if (err_msg) {
					*err_msg = att_format_unknown_option(arg);
					if (!*err_msg) {
						*err_msg = att_strdup("error: allocation failure");
					}
				}
				return 1;
			}
			continue;
		}
		if (strncmp(arg, "--output=", 9) == 0) {
			const char *value = arg + 9;
			char *copy = att_strdup(value);
			if (!copy) {
				if (err_msg) {
					*err_msg = att_strdup("error: allocation failure");
				}
				return -1;
			}
			if (out_opts->output_path) {
				free(out_opts->output_path);
			}
			out_opts->output_path = copy;
			continue;
		}
		if (strncmp(arg, "--timeout-ms=", 13) == 0) {
			const char *value = arg + 13;
			if (!*value) {
				if (err_msg) {
					*err_msg = att_strdup("error: invalid timeout value");
				}
				return 1;
			}
			char *endptr = NULL;
			long parsed = strtol(value, &endptr, 10);
			if (!endptr || *endptr != '\0' || parsed < 0 || parsed > INT_MAX) {
				if (err_msg) {
					*err_msg = att_strdup("error: invalid timeout value");
				}
				return 1;
			}
			out_opts->timeout_ms = (int)parsed;
			continue;
		}
		if (strncmp(arg, "--jobs=", 7) == 0) {
			const char *value = arg + 7;
			if (!*value) {
				if (err_msg) {
					*err_msg = att_strdup("error: invalid jobs value");
				}
				return 1;
			}
			/* Handle --jobs=auto */
			if (strcmp(value, "auto") == 0) {
				out_opts->jobs = att_get_cpu_count();
				continue;
			}
			/* Handle --jobs=N */
			char *endptr = NULL;
			long parsed = strtol(value, &endptr, 10);
			if (!endptr || *endptr != '\0' || parsed < 0 || parsed > INT_MAX) {
				if (err_msg) {
					*err_msg = att_strdup("error: invalid jobs value");
				}
				return 1;
			}
			/* Handle --jobs=0 as auto */
			if (parsed == 0) {
				out_opts->jobs = att_get_cpu_count();
			} else {
				out_opts->jobs = (int)parsed;
			}
			continue;
		}
		if (strncmp(arg, "--shuffle", 9) == 0) {
			const char *suffix = arg + 9;
			if (*suffix != '\0' && *suffix != '=') {
				/* Reject --shufflefoo, --shuffle123, etc. */
				if (err_msg) {
					*err_msg = att_format_unknown_option(arg);
					if (!*err_msg) {
						*err_msg = att_strdup("error: allocation failure");
					}
				}
				return 1;
			}
			out_opts->shuffle = true;
			if (*suffix == '=') {
				const char *value = suffix + 1;
				if (!*value) {
					if (err_msg) {
						*err_msg = att_strdup("error: invalid seed value");
					}
					return 1;
				}
				/* Reject negative values (strtoul accepts them) */
				if (*value == '-') {
					if (err_msg) {
						*err_msg = att_strdup("error: seed must be non-negative");
					}
					return 1;
				}
				char *endptr = NULL;
				unsigned long long parsed = strtoull(value, &endptr, 10);
				if (!endptr || *endptr != '\0') {
					if (err_msg) {
						*err_msg = att_strdup("error: invalid seed value");
					}
					return 1;
				}
				/* Reject values that overflow unsigned int */
				if (parsed > UINT_MAX) {
					if (err_msg) {
						*err_msg = att_strdup("error: seed value too large");
					}
					return 1;
				}
				out_opts->shuffle_seed = (unsigned int)parsed;
			} else {
				out_opts->shuffle_seed = (unsigned int)time(NULL);
			}
			continue;
		}
		if (err_msg) {
			*err_msg = att_format_unknown_option(arg);
			if (!*err_msg) {
				*err_msg = att_strdup("error: allocation failure");
			}
		}
		return 1;
	}

	if (out_opts->format == ATT_OUTPUT_JUNIT) {
		if (!out_opts->output_path) {
			out_opts->output_path = att_strdup("test_detail.xml");
			if (!out_opts->output_path) {
				if (err_msg) {
					*err_msg = att_strdup("error: allocation failure");
				}
				return -1;
			}
		}
	} else if (out_opts->output_path) {
		if (err_msg) {
			*err_msg = att_strdup("error: --output requires --format=junit");
			if (!*err_msg) {
				*err_msg = att_strdup("error: allocation failure");
			}
		}
		return 1;
	}

	if (err_msg) {
		*err_msg = NULL;
	}
	return 0;
}

static bool att_match_simple(const char *pattern, const char *text)
{
	if (*pattern == '\0') {
		return *text == '\0';
	}

	if (*pattern == '*') {
		while (*pattern == '*') {
			++pattern;
		}
		if (*pattern == '\0') {
			return true;
		}
		const char *t = text;
		while (*t) {
			if (att_match_simple(pattern, t)) {
				return true;
			}
			++t;
		}
		return false;
	}

	if (*pattern == '?') {
		return *text ? att_match_simple(pattern + 1, text + 1) : false;
	}

	if (*text != *pattern) {
		return false;
	}
	return att_match_simple(pattern + 1, text + 1);
}

bool att_filter_match(const att_test_case *test, const att_cli_options *opts)
{
	if (!test || !opts) {
		return false;
	}

	/* Step 1: Check positive filters */
	bool positive_match = false;
	if (!opts->filters || opts->filter_count == 0) {
		/* No positive filters means all tests are candidates */
		positive_match = true;
	} else {
		/* Check if test matches any positive filter */
		for (size_t i = 0; i < opts->filter_count; ++i) {
			if (att_match_simple(opts->filters[i], test->fullname)) {
				positive_match = true;
				break;
			}
		}
	}

	/* If test doesn't match positive filters, exclude it */
	if (!positive_match) {
		return false;
	}

	/* Step 2: Check negative filters */
	if (opts->negative_filters && opts->negative_filter_count > 0) {
		for (size_t i = 0; i < opts->negative_filter_count; ++i) {
			if (att_match_simple(opts->negative_filters[i], test->fullname)) {
				/* Test matches a negative filter, exclude it */
				return false;
			}
		}
	}

	/* Test passed both positive and negative filters */
	return true;
}

void att_print_list(const att_registry *registry, const att_cli_options *opts)
{
	if (!registry) {
		return;
	}
	for (size_t i = 0; i < registry->count; ++i) {
		const att_test_case *test = &registry->tests[i];
		if (opts && !att_filter_match(test, opts)) {
			continue;
		}
		printf("%s\n", test->fullname);
	}
}

void att_cli_dispose(att_cli_options *opts)
{
	if (!opts) {
		return;
	}
	if (opts->filters) {
		for (size_t i = 0; i < opts->filter_count; ++i) {
			free(opts->filters[i]);
		}
		free(opts->filters);
	}
	if (opts->negative_filters) {
		for (size_t i = 0; i < opts->negative_filter_count; ++i) {
			free(opts->negative_filters[i]);
		}
		free(opts->negative_filters);
	}
	if (opts->filter_raw) {
		free((void *)opts->filter_raw);
	}
	if (opts->output_path) {
		free(opts->output_path);
	}
	opts->filters = NULL;
	opts->filter_count = 0;
	opts->negative_filters = NULL;
	opts->negative_filter_count = 0;
	opts->filter_raw = NULL;
	opts->list_only = false;
	opts->help_requested = false;
	opts->color_enabled = true;
	opts->output_path = NULL;
	opts->format = ATT_OUTPUT_DEFAULT;
	opts->timeout_ms = 0;
}

void att_print_help(const char *program_name)
{
	if (!program_name) {
		program_name = "attest";
	}
	printf("Usage: %s [OPTIONS]\n\n", program_name);
	printf("A C11 unit testing framework inspired by GoogleTest.\n\n");
	printf("Options:\n");
	printf("  -h, --help              Display this help message and exit\n");
	printf("  --list                  List all available tests and exit\n");
	printf("  --filter=PATTERN        Run tests matching PATTERN (wildcards: * and ?)\n");
	printf("                          Multiple patterns can be separated by ';'\n");
	printf("                          Shorthand: 'Suite' -> 'Suite.*', '.Name' -> '*.Name'\n");
	printf("                          Prefix with '-' to exclude: 'Math.*;-Math.Slow*'\n");
	printf("  --shuffle[=SEED]        Randomize test order with optional seed for reproducibility\n");
	printf("  --no-color              Disable colored output\n");
	printf("  --format=FORMAT         Output format: default, tap, or junit\n");
	printf("  --output=FILE           Write output to FILE (requires --format=junit)\n");
	printf("  --timeout-ms=MS         Set test timeout in milliseconds\n");
	printf("  --jobs=N                Run tests in parallel with N workers\n");
	printf("                          Use 'auto' or 0 to auto-detect CPU count\n");
	printf("\n");
	printf("Exit codes:\n");
	printf("  0  All tests passed or --list mode\n");
	printf("  1  One or more test failures\n");
	printf("  2  CLI parsing error\n");
	printf("  3  Initialization failure\n");
	printf("\n");
	printf("Examples:\n");
	printf("  %s                       Run all tests\n", program_name);
	printf("  %s --list                List all tests\n", program_name);
	printf("  %s --filter=Suite.*      Run all tests in Suite\n", program_name);
	printf("  %s --filter=*.Name       Run all tests named Name\n", program_name);
	printf("  %s --jobs=auto           Run tests in parallel (auto-detect CPUs)\n", program_name);
}
