#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attest/attest.h"
#include "internal/attest_internal.h"

static att_registry g_registry;

att_registry* att_registry_get(void)
{
	return &g_registry;
}

const char* att_registry_error(void)
{
	return g_registry.last_error;
}

static int att_registry_grow(att_registry* registry)
{
	size_t new_capacity = registry->capacity ? registry->capacity * 2 : 16;
	void* block = realloc(registry->tests, new_capacity * sizeof(att_test_case));
	if (!block) {
		return -1;
	}
	registry->tests = block;
	registry->capacity = new_capacity;
	return 0;
}

int att_registry_add(const char* suite, const char* name, att_test_fn fn, const char* file, int line, const char** error)
{
	att_registry* registry = &g_registry;
	if (registry->frozen) {
		if (error) {
			*error = "registry is frozen";
		}
		return -1;
	}

	if (!suite || !name || !fn) {
		if (error) {
			*error = "invalid test registration";
		}
		return -1;
	}

	size_t suite_len = strlen(suite);
	size_t name_len = strlen(name);
	size_t fullname_len = suite_len + 1 + name_len;

	for (size_t i = 0; i < registry->count; ++i) {
		const att_test_case* existing = &registry->tests[i];
		if (strcmp(existing->suite, suite) == 0 && strcmp(existing->name, name) == 0) {
			free((void*)registry->last_error);
			size_t msg_len = fullname_len + sizeof("error: duplicate test name ''");
			char* msg = malloc(msg_len);
			if (msg) {
				snprintf(msg, msg_len, "error: duplicate test name '%s.%s'", suite, name);
			}
			registry->last_error = msg;
			if (error) {
				*error = registry->last_error;
			}
			return 1;
		}
	}

	if (registry->count == registry->capacity) {
		if (att_registry_grow(registry) != 0) {
			if (error) {
				*error = "allocation failure";
			}
			return -1;
		}
	}

	char* fullname = malloc(fullname_len + 1);
	if (!fullname) {
		if (error) {
			*error = "allocation failure";
		}
		return -1;
	}
	memcpy(fullname, suite, suite_len);
	fullname[suite_len] = '.';
	memcpy(fullname + suite_len + 1, name, name_len);
	fullname[fullname_len] = '\0';

	att_test_case* slot = &registry->tests[registry->count++];
	slot->suite = suite;
	slot->name = name;
	slot->fullname = fullname;
	slot->file = file;
	slot->line = line;
	slot->fn = fn;

	if (error) {
		*error = NULL;
	}
	return 0;
}

void att_registry_finalize(void)
{
	g_registry.frozen = true;
}

void att_register_test(const char* suite, const char* name, att_test_fn fn, const char* file, int line)
{
	att_registry_add(suite, name, fn, file, line, NULL);
}

void att_register_manual(const att_register_fn* fns, size_t count)
{
	if (!fns) {
		return;
	}
	for (size_t i = 0; i < count; ++i) {
		if (fns[i]) {
			fns[i]();
		}
	}
}
