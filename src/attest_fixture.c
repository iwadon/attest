#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attest/attest.h"
#include "internal/attest_context.h"

typedef struct att_fixture_entry {
	const char *name;
	size_t size;
	att_fixture_hook setup;
	att_fixture_hook teardown;
} att_fixture_entry;

static att_fixture_entry *g_fixture_entries;
static size_t g_fixture_entry_count;
static size_t g_fixture_entry_capacity;

static att_fixture_entry *att_fixture_entry_find(const char *name)
{
	for (size_t i = 0; i < g_fixture_entry_count; ++i) {
		att_fixture_entry *entry = &g_fixture_entries[i];
		if (entry->name && strcmp(entry->name, name) == 0) {
			return entry;
		}
	}
	return NULL;
}

static att_fixture_entry *att_fixture_entry_ensure(const char *name, size_t size)
{
	att_fixture_entry *entry = att_fixture_entry_find(name);
	if (entry) {
		if (entry->size == 0) {
			entry->size = size;
		} else if (entry->size != size) {
			fprintf(stderr, "error: fixture '%s' registered with conflicting size\n", name);
			abort();
		}
		return entry;
	}

	if (g_fixture_entry_count == g_fixture_entry_capacity) {
		size_t new_capacity = g_fixture_entry_capacity ? g_fixture_entry_capacity * 2 : 8;
		void *grown = realloc(g_fixture_entries, new_capacity * sizeof(*g_fixture_entries));
		if (!grown) {
			return NULL;
		}
		g_fixture_entries = grown;
		g_fixture_entry_capacity = new_capacity;
	}

	entry = &g_fixture_entries[g_fixture_entry_count++];
	entry->name = name;
	entry->size = size;
	entry->setup = NULL;
	entry->teardown = NULL;
	return entry;
}

void att_fixture_register_setup(const char *fixture_name, size_t fixture_size, att_fixture_hook fn)
{
	if (!fixture_name || fixture_size == 0 || !fn) {
		return;
	}
	att_fixture_entry *entry = att_fixture_entry_ensure(fixture_name, fixture_size);
	if (!entry) {
		fprintf(stderr, "error: failed to register setup for fixture '%s'\n", fixture_name);
		abort();
	}
	entry->setup = fn;
}

void att_fixture_register_teardown(const char *fixture_name, size_t fixture_size, att_fixture_hook fn)
{
	if (!fixture_name || fixture_size == 0 || !fn) {
		return;
	}
	att_fixture_entry *entry = att_fixture_entry_ensure(fixture_name, fixture_size);
	if (!entry) {
		fprintf(stderr, "error: failed to register teardown for fixture '%s'\n", fixture_name);
		abort();
	}
	entry->teardown = fn;
}

static att_fixture_entry *att_fixture_entry_require(const char *fixture_name, size_t fixture_size)
{
	att_fixture_entry *entry = att_fixture_entry_ensure(fixture_name, fixture_size);
	if (!entry) {
		att_handle_truth("ASSERT_TRUE(att_fixture_entry_ensure != NULL)", __FILE__, __LINE__, true, false, true,
			"att_fixture_entry_ensure != NULL");
		att_context_abort();
		return NULL;
	}
	return entry;
}

void att_fixture_run(const char *fixture_name, size_t fixture_size, att_fixture_body_fn body_fn)
{
	if (!body_fn) {
		return;
	}

	att_fixture_entry *entry = att_fixture_entry_require(fixture_name, fixture_size);
	if (!entry) {
		return;
	}

	void *instance = calloc(1, fixture_size);
	if (!instance) {
		att_handle_truth("ASSERT_TRUE(att_fixture_instance != NULL)", __FILE__, __LINE__, true, instance != NULL, true,
			"att_fixture_instance != NULL");
		att_context_abort();
		return;
	}

	att_context_fixture_enter(fixture_name, fixture_size, instance, entry->teardown);

	if (entry->setup) {
		entry->setup(instance);
	}

	att_context_phase_set(ATT_CONTEXT_PHASE_TEST);
	body_fn(instance);

	att_context_phase_set(ATT_CONTEXT_PHASE_TEARDOWN);
	if (entry->teardown) {
		att_context_fixture_mark_teardown_started();
		entry->teardown(instance);
	}

	att_context_fixture_cleanup();
}
