#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	if (!filters) {
		free(raw_copy);
		if (err_msg) {
			*err_msg = att_strdup("error: allocation failure");
		}
		return -1;
	}

	size_t index = 0;
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
		char *normalized = att_normalize_pattern(slice);
		free(slice);
		if (!normalized) {
			if (err_msg) {
				*err_msg = att_strdup("error: allocation failure");
			}
			goto fail;
		}
		filters[index++] = normalized;
		if (!delim) {
			break;
		}
		head = delim + 1;
	}

	opts->filter_raw = raw_copy;
	opts->filters = filters;
	opts->filter_count = index;
	return 0;

fail:
	for (size_t i = 0; i < index; ++i) {
		free(filters[i]);
	}
	free(filters);
	free(raw_copy);
	return -1;
}

int att_cli_parse(int argc, char **argv, att_cli_options *out_opts, char **err_msg)
{
	if (!out_opts) {
		if (err_msg) {
			char *msg = att_strdup("error: internal");
			if (!msg) {
				msg = att_strdup("error: allocation failure");
			}
			*err_msg = msg;
		}
		return -1;
	}

	out_opts->list_only = false;
	out_opts->color_enabled = true;
	out_opts->filter_raw = NULL;
	out_opts->filters = NULL;
	out_opts->filter_count = 0;

	for (int i = 1; i < argc; ++i) {
		const char *arg = argv[i];
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
		if (err_msg) {
			*err_msg = att_format_unknown_option(arg);
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
	if (!opts->filters || opts->filter_count == 0) {
		return true;
	}
	for (size_t i = 0; i < opts->filter_count; ++i) {
		if (att_match_simple(opts->filters[i], test->fullname)) {
			return true;
		}
	}
	return false;
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
	if (opts->filter_raw) {
		free((void *)opts->filter_raw);
	}
	opts->filters = NULL;
	opts->filter_count = 0;
	opts->filter_raw = NULL;
	opts->list_only = false;
	opts->color_enabled = true;
}
