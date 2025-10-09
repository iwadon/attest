#ifndef ATTEST_CONTEXT_H
#define ATTEST_CONTEXT_H

#include <stdbool.h>
#include <setjmp.h>

#include "internal/attest_internal.h"

typedef struct att_test_result {
	int assertions_total;
	int fail_fatal;
	int fail_nonfatal;
	bool aborted;
} att_test_result;

void att_context_begin(const att_test_case* test, bool color_enabled);
int att_context_protect(void);
void att_context_end(att_test_result* out_result);
void att_context_record_assert(bool fatal, bool passed);
void att_context_register_failure(bool fatal);
void att_context_abort(void);
bool att_context_color_enabled(void);
const att_test_case* att_context_current_test(void);

#endif /* ATTEST_CONTEXT_H */
