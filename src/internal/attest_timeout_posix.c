/*
 * POSIX timeout implementation using SIGALRM and setitimer.
 * Also includes Human68k stubs (timeout not supported).
 */

#include "internal/attest_timeout.h"

#if defined(ATT_PLATFORM_POSIX)

#include <signal.h>
#include <string.h>
#include <sys/time.h>

static ATT_THREAD_LOCAL bool g_timeout_handler_installed;

static void att_timeout_signal_handler(int signo)
{
	(void)signo;
	if (!att_timeout_ctx_is_active()) {
		return;
	}
	att_timeout_ctx_set_triggered(true);
	att_timeout_ctx_abort();
}

static void att_timeout_install_handler(void)
{
	if (g_timeout_handler_installed) {
		return;
	}
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = att_timeout_signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGALRM, &sa, NULL) == 0) {
		g_timeout_handler_installed = true;
	}
}

void att_timeout_start(int timeout_ms)
{
	if (!att_timeout_ctx_is_active() || timeout_ms <= 0) {
		return;
	}
	att_timeout_install_handler();
	att_timeout_ctx_set_triggered(false);
	att_timeout_ctx_set_ms(timeout_ms);

	struct itimerval timer;
	memset(&timer, 0, sizeof(timer));
	timer.it_value.tv_sec = timeout_ms / 1000;
	timer.it_value.tv_usec = (timeout_ms % 1000) * 1000;
	setitimer(ITIMER_REAL, &timer, NULL);
}

void att_timeout_stop(void)
{
	struct itimerval timer;
	memset(&timer, 0, sizeof(timer));
	setitimer(ITIMER_REAL, &timer, NULL);
	att_timeout_ctx_set_ms(0);
}

#elif defined(ATT_PLATFORM_HUMAN68K)
/*
 * Human68k: Timeout feature not supported (no setitimer/sigaction available)
 */

void att_timeout_start(int timeout_ms)
{
	(void)timeout_ms;
	/* No-op: timeout not supported on Human68k */
}

void att_timeout_stop(void)
{
	/* No-op: timeout not supported on Human68k */
}

#endif /* ATT_PLATFORM_POSIX / ATT_PLATFORM_HUMAN68K */
