#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attest/attest.h"

#include "internal/attest_internal.h"

#if defined(ATT_PLATFORM_HUMAN68K)
/* Human68k: capture not supported (no dup/dup2 available) */
#define ATT_CAPTURE_DISABLED
#elif defined(_WIN32)
#include <io.h>
#define ATT_DUP _dup
#define ATT_DUP2 _dup2
#define ATT_CLOSE _close
#define ATT_FILENO _fileno
#else
#include <unistd.h>
#define ATT_DUP dup
#define ATT_DUP2 dup2
#define ATT_CLOSE close
#define ATT_FILENO fileno
#endif

/* Capture serialization.
 *
 * att_capture_{begin,end} swap stderr's file descriptor via dup2 against a
 * process-global state struct (g_capture). When --jobs>1 runs subtests in
 * parallel, ATT_EXPECT_SUBTEST_FAILS on multiple workers race on this
 * single FD: late beginners either lose their captures (subtest failures
 * leak to the real stderr) or wedge the FD entirely (hang). A
 * process-global mutex held across begin..end serializes the critical
 * section so only one worker captures at a time; others block at begin
 * until the active capture ends.
 *
 * Nested capture on the same thread (e.g. ATT_EXPECT_SUBTEST_FAILS inside
 * another ATT_EXPECT_SUBTEST_FAILS) was historically rejected with -1 by
 * the active-flag check. Because the surrounding mutex is non-recursive,
 * we MUST detect self-nesting before attempting to lock — otherwise the
 * inner begin deadlocks waiting for the lock the outer call already
 * holds. We track the owning thread id and short-circuit when it matches
 * the current thread. */
#if defined(ATT_THREADS_POSIX)
#include <pthread.h>
static pthread_mutex_t g_capture_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_capture_owner;
static int g_capture_owner_set;
#define ATT_CAPTURE_LOCK() pthread_mutex_lock(&g_capture_mutex)
#define ATT_CAPTURE_UNLOCK() pthread_mutex_unlock(&g_capture_mutex)
#define ATT_CAPTURE_OWNER_IS_SELF() \
	(g_capture_owner_set && pthread_equal(g_capture_owner, pthread_self()))
#define ATT_CAPTURE_OWNER_TAKE()          \
	do {                                  \
		g_capture_owner = pthread_self(); \
		g_capture_owner_set = 1;          \
	} while (0)
#define ATT_CAPTURE_OWNER_RELEASE() (g_capture_owner_set = 0)
#elif defined(ATT_THREADS_C11)
#include <threads.h>
static mtx_t g_capture_mutex;
static once_flag g_capture_mutex_once = ONCE_FLAG_INIT;
static thrd_t g_capture_owner;
static int g_capture_owner_set;
static void att_capture_mutex_init(void) { mtx_init(&g_capture_mutex, mtx_plain); }
#define ATT_CAPTURE_LOCK()                                        \
	do {                                                          \
		call_once(&g_capture_mutex_once, att_capture_mutex_init); \
		mtx_lock(&g_capture_mutex);                               \
	} while (0)
#define ATT_CAPTURE_UNLOCK() mtx_unlock(&g_capture_mutex)
#define ATT_CAPTURE_OWNER_IS_SELF() \
	(g_capture_owner_set && thrd_equal(g_capture_owner, thrd_current()))
#define ATT_CAPTURE_OWNER_TAKE()          \
	do {                                  \
		g_capture_owner = thrd_current(); \
		g_capture_owner_set = 1;          \
	} while (0)
#define ATT_CAPTURE_OWNER_RELEASE() (g_capture_owner_set = 0)
#elif defined(ATT_THREADS_WIN32)
#include <windows.h>
static CRITICAL_SECTION g_capture_cs;
static INIT_ONCE g_capture_cs_once = INIT_ONCE_STATIC_INIT;
static DWORD g_capture_owner;
static int g_capture_owner_set;
static BOOL CALLBACK att_capture_cs_init(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
	(void)once;
	(void)param;
	(void)ctx;
	InitializeCriticalSection(&g_capture_cs);
	return TRUE;
}
#define ATT_CAPTURE_LOCK()                                                        \
	do {                                                                          \
		InitOnceExecuteOnce(&g_capture_cs_once, att_capture_cs_init, NULL, NULL); \
		EnterCriticalSection(&g_capture_cs);                                      \
	} while (0)
#define ATT_CAPTURE_UNLOCK() LeaveCriticalSection(&g_capture_cs)
#define ATT_CAPTURE_OWNER_IS_SELF() \
	(g_capture_owner_set && g_capture_owner == GetCurrentThreadId())
#define ATT_CAPTURE_OWNER_TAKE()                \
	do {                                        \
		g_capture_owner = GetCurrentThreadId(); \
		g_capture_owner_set = 1;                \
	} while (0)
#define ATT_CAPTURE_OWNER_RELEASE() (g_capture_owner_set = 0)
#else
#define ATT_CAPTURE_LOCK() ((void)0)
#define ATT_CAPTURE_UNLOCK() ((void)0)
#define ATT_CAPTURE_OWNER_IS_SELF() (g_capture.active)
#define ATT_CAPTURE_OWNER_TAKE() ((void)0)
#define ATT_CAPTURE_OWNER_RELEASE() ((void)0)
#endif

#ifdef ATT_CAPTURE_DISABLED
/* Human68k: Stub implementations for disabled capture functionality */

int att_capture_begin(void)
{
	/* No-op: capture not supported */
	return -1;
}

int att_capture_end(att_captured *out)
{
	if (out) {
		out->data = NULL;
		out->size = 0;
	}
	/* No-op: capture not supported */
	return -1;
}

#else
/* Full capture implementation for platforms with dup/dup2 support */

typedef struct att_capture_state {
	int original_fd;
	FILE *temp;
	int active;
} att_capture_state;

static att_capture_state g_capture = { .original_fd = -1 };

static void att_capture_state_reset(void)
{
	g_capture.active = 0;
	g_capture.temp = NULL;
	g_capture.original_fd = -1;
}

int att_capture_begin(void)
{
	/* Reject self-nesting BEFORE touching the (non-recursive) mutex —
	 * otherwise the inner call deadlocks waiting on the lock the outer
	 * call already holds. */
	if (ATT_CAPTURE_OWNER_IS_SELF()) {
		return -1;
	}

	ATT_CAPTURE_LOCK();

	if (g_capture.active) {
		/* Another thread released the lock between our owner check and the
		 * lock acquisition but somehow left active set. Treat as a hard
		 * error rather than corrupting their state. */
		ATT_CAPTURE_UNLOCK();
		return -1;
	}

	fflush(stderr);

	FILE *temp = tmpfile();
	if (!temp) {
		ATT_CAPTURE_UNLOCK();
		return -1;
	}

	int temp_fd = ATT_FILENO(temp);
	int stderr_fd = ATT_FILENO(stderr);
	int saved_fd = ATT_DUP(stderr_fd);
	if (saved_fd < 0) {
		fclose(temp);
		ATT_CAPTURE_UNLOCK();
		return -1;
	}

	if (ATT_DUP2(temp_fd, stderr_fd) < 0) {
		ATT_CLOSE(saved_fd);
		fclose(temp);
		att_capture_state_reset();
		ATT_CAPTURE_UNLOCK();
		return -1;
	}

	g_capture.active = 1;
	g_capture.temp = temp;
	g_capture.original_fd = saved_fd;
	ATT_CAPTURE_OWNER_TAKE();
	/* Lock is held until matching att_capture_end. */
	return 0;
}

int att_capture_end(att_captured *out)
{
	if (out) {
		out->data = NULL;
		out->size = 0;
	}

	int rc = -1;
	char *buffer = NULL;
	size_t read = 0;

	/* The lock is held iff *this* thread is the current owner — i.e. a
	 * matching begin() on this thread succeeded. Checking ownership rather
	 * than the active flag avoids unlocking a mutex we never acquired
	 * (e.g. end() called without a successful begin(), or called after a
	 * nested begin() returned -1). */
	int lock_held = ATT_CAPTURE_OWNER_IS_SELF();

	if (!lock_held || !g_capture.temp) {
		goto cleanup;
	}

	fflush(stderr);

	int stderr_fd = ATT_FILENO(stderr);
	if (ATT_DUP2(g_capture.original_fd, stderr_fd) < 0) {
		goto cleanup;
	}
	ATT_CLOSE(g_capture.original_fd);
	g_capture.original_fd = -1;

	long end_pos = ftell(g_capture.temp);
	if (end_pos < 0) {
		goto cleanup;
	}

	if (fseek(g_capture.temp, 0L, SEEK_SET) != 0) {
		goto cleanup;
	}

	size_t size = (size_t)end_pos;
	buffer = malloc(size + 1);
	if (!buffer) {
		goto cleanup;
	}

	read = fread(buffer, 1, size, g_capture.temp);
	buffer[read] = '\0';
	rc = 0;

cleanup:
	if (g_capture.temp) {
		fclose(g_capture.temp);
	}
	if (g_capture.original_fd >= 0) {
		ATT_CLOSE(g_capture.original_fd);
	}
	att_capture_state_reset();

	if (rc == 0 && out) {
		out->data = buffer;
		out->size = read;
	} else {
		free(buffer);
	}

	if (lock_held) {
		ATT_CAPTURE_OWNER_RELEASE();
		ATT_CAPTURE_UNLOCK();
	}

	return rc;
}

#endif /* ATT_CAPTURE_DISABLED */
