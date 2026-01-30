/*
 * Windows timeout implementation using threads and events.
 */

#include "internal/attest_timeout.h"

#ifdef ATT_PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <process.h>
#include <stdio.h>
#include <windows.h>

static unsigned __stdcall att_timeout_thread_proc(void *arg)
{
	int timeout_ms = (int)(intptr_t)arg;
	HANDLE event = (HANDLE)att_timeout_ctx_get_event();

	/* Use WaitForSingleObject instead of Sleep to allow early cancellation */
	if (event) {
		DWORD wait_result = WaitForSingleObject(event, (DWORD)timeout_ms);

		/* WAIT_TIMEOUT means the timeout expired (expected path) */
		/* WAIT_OBJECT_0 means the event was signaled (early cancellation) */
		if (wait_result == WAIT_TIMEOUT && att_timeout_ctx_is_active() && !att_timeout_ctx_is_triggered()) {
			att_timeout_ctx_set_triggered(true);
			/* Signal the event to indicate timeout occurred */
			SetEvent(event);
		}
	}

	return 0;
}

void att_timeout_start(int timeout_ms)
{
	if (!att_timeout_ctx_is_active() || timeout_ms <= 0) {
		return;
	}

	/* Clean up any existing timeout */
	att_timeout_stop();

	att_timeout_ctx_set_triggered(false);
	att_timeout_ctx_set_ms(timeout_ms);
	att_timeout_ctx_set_init_failed(false);

	/* Create event for timeout signaling */
	HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!event) {
		fprintf(stderr, "Warning: Failed to create timeout event. Timeout feature disabled.\n");
		att_timeout_ctx_set_init_failed(true);
		att_timeout_ctx_set_ms(0);
		return;
	}
	att_timeout_ctx_set_event(event);

	/* Create timeout thread */
	HANDLE thread = (HANDLE)_beginthreadex(
		NULL,
		0,
		att_timeout_thread_proc,
		(void *)(intptr_t)timeout_ms,
		0,
		NULL);

	if (!thread) {
		fprintf(stderr, "Warning: Failed to create timeout thread. Timeout feature disabled.\n");
		CloseHandle(event);
		att_timeout_ctx_set_event(NULL);
		att_timeout_ctx_set_init_failed(true);
		att_timeout_ctx_set_ms(0);
		return;
	}
	att_timeout_ctx_set_thread(thread);
}

void att_timeout_stop(void)
{
	HANDLE thread = (HANDLE)att_timeout_ctx_get_thread();
	HANDLE event = (HANDLE)att_timeout_ctx_get_event();

	if (thread) {
		/* Signal the event to wake up the timeout thread early */
		if (event) {
			SetEvent(event);
		}

		/* Wait for thread to complete with reasonable timeout (1 second should be enough) */
		DWORD wait_result = WaitForSingleObject(thread, 1000);
		if (wait_result == WAIT_TIMEOUT) {
			/* Thread didn't finish in time - this shouldn't happen with the new implementation
			   but we handle it gracefully to avoid hanging */
			fprintf(stderr, "Warning: Timeout thread did not terminate in time\n");
		}
		CloseHandle(thread);
		att_timeout_ctx_set_thread(NULL);
	}

	if (event) {
		CloseHandle(event);
		att_timeout_ctx_set_event(NULL);
	}

	att_timeout_ctx_set_ms(0);
}

#endif /* ATT_PLATFORM_WINDOWS */
