#ifndef ATTEST_TIMEOUT_H
#define ATTEST_TIMEOUT_H

#include <stdbool.h>

#include "attest/attest.h"
#include "internal/attest_internal.h"

/* ========================================================================
 * Platform-agnostic timeout interface
 *
 * These functions manage test timeouts. The implementation varies by platform:
 * - POSIX: SIGALRM + setitimer with longjmp on timeout
 * - Windows: Worker thread + event signaling with polling
 * - Human68k: No-op stubs (timeout not supported)
 *
 * The timeout module needs access to the test context for:
 * - Setting timeout_triggered flag
 * - Reading active flag
 * - Performing longjmp on timeout (POSIX)
 * - Managing Windows handles (stored in context)
 *
 * To avoid exposing the full context structure, we use accessor functions
 * defined in attest_assert.c that the timeout implementations call.
 * ======================================================================== */

/*
 * Start a timeout timer for the current test.
 * If the test exceeds timeout_ms, the timeout is triggered.
 *
 * timeout_ms: Timeout duration in milliseconds (0 or negative = no timeout)
 */
void att_timeout_start(int timeout_ms);

/*
 * Stop and clean up the current timeout timer.
 * Safe to call even if no timeout is active.
 */
void att_timeout_stop(void);

/* ========================================================================
 * Context accessors (implemented in attest_assert.c)
 * Used by platform-specific timeout implementations
 * ======================================================================== */

/*
 * Check if the current context is active.
 */
bool att_timeout_ctx_is_active(void);

/*
 * Get the timeout_triggered flag.
 */
bool att_timeout_ctx_is_triggered(void);

/*
 * Set the timeout_triggered flag.
 */
void att_timeout_ctx_set_triggered(bool triggered);

/*
 * Get the timeout_ms value.
 */
int att_timeout_ctx_get_ms(void);

/*
 * Set the timeout_ms value.
 */
void att_timeout_ctx_set_ms(int ms);

/*
 * Perform longjmp to abort the current test (POSIX timeout handler).
 */
void att_timeout_ctx_abort(void);

#ifdef ATT_PLATFORM_WINDOWS
/*
 * Windows-specific: Get/set timeout handles.
 */
void *att_timeout_ctx_get_thread(void);
void att_timeout_ctx_set_thread(void *handle);
void *att_timeout_ctx_get_event(void);
void att_timeout_ctx_set_event(void *handle);
bool att_timeout_ctx_init_failed(void);
void att_timeout_ctx_set_init_failed(bool failed);
#endif

#endif /* ATTEST_TIMEOUT_H */
