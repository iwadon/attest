#ifndef ATTEST_CONTEXT_H
#define ATTEST_CONTEXT_H

#include <setjmp.h>
#include <stdbool.h>

#include "internal/attest_internal.h"

typedef struct att_test_result {
	int assertions_total;
	int fail_fatal;
	int fail_nonfatal;
	bool aborted;
	bool skipped;
	char *skip_reason;
	char *failure_log;
	bool timed_out;
	int timeout_ms;
} att_test_result;

typedef enum att_context_phase {
	ATT_CONTEXT_PHASE_NONE = 0,
	ATT_CONTEXT_PHASE_SETUP,
	ATT_CONTEXT_PHASE_TEST,
	ATT_CONTEXT_PHASE_TEARDOWN
} att_context_phase;

void att_context_begin(const att_test_case *test, bool color_enabled, att_output_format format);
int att_context_protect(void);
void att_context_end(att_test_result *out_result);
void att_context_record_assert(bool fatal, bool passed);
void att_context_register_failure(bool fatal);
void att_context_abort(void);
bool att_context_color_enabled(void);
const att_test_case *att_context_current_test(void);
void att_context_phase_set(att_context_phase phase);
att_context_phase att_context_phase_current(void);
void att_context_fixture_enter(const char *fixture_name, size_t fixture_size, void *instance, att_fixture_hook teardown);
void att_context_fixture_mark_teardown_started(void);
void att_context_fixture_cleanup(void);
void att_context_fixture_on_abort(void);
void att_context_skip(const char *reason);
void att_context_capture_failures(bool enabled);
void att_context_timeout_start(int timeout_ms);
void att_context_timeout_stop(void);
att_output_format att_context_get_format(void);

#endif /* ATTEST_CONTEXT_H */
