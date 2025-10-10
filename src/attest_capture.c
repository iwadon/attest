#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attest/attest.h"

#if defined(_WIN32)
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
	if (g_capture.active) {
		return -1;
	}

	fflush(stderr);

	FILE *temp = tmpfile();
	if (!temp) {
		return -1;
	}

	int temp_fd = ATT_FILENO(temp);
	int stderr_fd = ATT_FILENO(stderr);
	int saved_fd = ATT_DUP(stderr_fd);
	if (saved_fd < 0) {
		fclose(temp);
		return -1;
	}

	if (ATT_DUP2(temp_fd, stderr_fd) < 0) {
		ATT_CLOSE(saved_fd);
		fclose(temp);
		att_capture_state_reset();
		return -1;
	}

	g_capture.active = 1;
	g_capture.temp = temp;
	g_capture.original_fd = saved_fd;
	return 0;
}

int att_capture_end(att_captured *out)
{
	if (!g_capture.active || !g_capture.temp) {
		if (out) {
			out->data = NULL;
			out->size = 0;
		}
		att_capture_state_reset();
		return -1;
	}

	fflush(stderr);

	int stderr_fd = ATT_FILENO(stderr);
	if (ATT_DUP2(g_capture.original_fd, stderr_fd) < 0) {
		ATT_CLOSE(g_capture.original_fd);
		fclose(g_capture.temp);
		att_capture_state_reset();
		if (out) {
			out->data = NULL;
			out->size = 0;
		}
		return -1;
	}
	ATT_CLOSE(g_capture.original_fd);
	g_capture.original_fd = -1;

	long end_pos = ftell(g_capture.temp);
	if (end_pos < 0) {
		fclose(g_capture.temp);
		att_capture_state_reset();
		if (out) {
			out->data = NULL;
			out->size = 0;
		}
		return -1;
	}

	if (fseek(g_capture.temp, 0L, SEEK_SET) != 0) {
		fclose(g_capture.temp);
		att_capture_state_reset();
		if (out) {
			out->data = NULL;
			out->size = 0;
		}
		return -1;
	}

	size_t size = (size_t)end_pos;
	char *buffer = malloc(size + 1);
	if (!buffer) {
		fclose(g_capture.temp);
		att_capture_state_reset();
		if (out) {
			out->data = NULL;
			out->size = 0;
		}
		return -1;
	}

	size_t read = fread(buffer, 1, size, g_capture.temp);
	buffer[read] = '\0';

	fclose(g_capture.temp);
	att_capture_state_reset();

	if (out) {
		out->data = buffer;
		out->size = read;
	} else {
		free(buffer);
	}

	return 0;
}
