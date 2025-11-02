#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attest/attest.h"
#include "internal/attest_context.h"
#include "internal/attest_internal.h"

#ifdef ATT_THREADS_POSIX
#include <pthread.h>
#endif

/* ========================================================================
 * Data Structures for Parallel Execution
 * ======================================================================== */

/* Per-test result structure for parallel execution */
struct att_parallel_result {
	const att_test_case *test;
	att_test_result result;
	bool completed;
	char *output_buffer; /* Output capture buffer (for Phase 2.4) */
	size_t output_size;
};

#ifdef ATT_THREADS_POSIX

/* Worker pool shared state */
struct att_worker_pool {
	size_t next_test_index; /* Next test index to execute (mutex-protected) */
	size_t total_tests;		/* Total number of tests to execute */
	pthread_mutex_t lock;	/* Mutex protecting next_test_index */
	const att_registry *registry;
	const att_cli_options *options;
	att_parallel_result *results; /* Results array (one per test) */
};

/* Individual worker thread */
struct att_worker {
	pthread_t thread;
	att_worker_pool *pool;
	int worker_id;
};

/* ========================================================================
 * Worker Pool Initialization and Cleanup
 * ======================================================================== */

/**
 * Initialize worker pool structure
 *
 * @param pool Pointer to worker pool structure
 * @param registry Test registry
 * @param options CLI options
 * @param total_tests Total number of filtered tests to execute
 * @return 0 on success, -1 on failure
 */
int att_worker_pool_init(att_worker_pool *pool, const att_registry *registry,
	const att_cli_options *options, size_t total_tests)
{
	if (!pool || !registry || !options || total_tests == 0) {
		return -1;
	}

	memset(pool, 0, sizeof(*pool));

	pool->next_test_index = 0;
	pool->total_tests = total_tests;
	pool->registry = registry;
	pool->options = options;

	/* Allocate results array */
	pool->results = calloc(total_tests, sizeof(att_parallel_result));
	if (!pool->results) {
		return -1;
	}

	/* Initialize mutex */
	if (pthread_mutex_init(&pool->lock, NULL) != 0) {
		free(pool->results);
		pool->results = NULL;
		return -1;
	}

	return 0;
}

/**
 * Destroy worker pool and free resources
 *
 * @param pool Pointer to worker pool structure
 */
void att_worker_pool_destroy(att_worker_pool *pool)
{
	if (!pool) {
		return;
	}

	/* Destroy mutex */
	pthread_mutex_destroy(&pool->lock);

	/* Free result buffers */
	if (pool->results) {
		for (size_t i = 0; i < pool->total_tests; i++) {
			/* Free output buffer (was failure_log, ownership transferred) */
			if (pool->results[i].output_buffer) {
				free(pool->results[i].output_buffer);
				pool->results[i].output_buffer = NULL;
			}
			/* Free skip_reason if present */
			if (pool->results[i].result.skip_reason) {
				free(pool->results[i].result.skip_reason);
				pool->results[i].result.skip_reason = NULL;
			}
		}
		free(pool->results);
		pool->results = NULL;
	}

	memset(pool, 0, sizeof(*pool));
}

#endif /* ATT_THREADS_POSIX */

/* ========================================================================
 * Fallback for Non-Threaded Environments
 * ======================================================================== */

#ifdef ATT_THREADS_NONE

/**
 * Dummy initialization function for non-threaded environments
 * Always returns failure to indicate parallel execution is not supported
 */
int att_worker_pool_init(void *pool, const void *registry,
	const void *options, size_t total_tests)
{
	(void)pool;
	(void)registry;
	(void)options;
	(void)total_tests;
	return -1; /* Parallel execution not supported */
}

/**
 * Dummy cleanup function for non-threaded environments
 */
void att_worker_pool_destroy(void *pool)
{
	(void)pool;
}

#endif /* ATT_THREADS_NONE */

/* ========================================================================
 * Worker Thread Main Function (POSIX)
 * ======================================================================== */

#ifdef ATT_THREADS_POSIX

/**
 * Worker thread main function
 * Pulls tests from the shared queue and executes them independently
 *
 * @param arg Pointer to att_worker structure
 * @return NULL (unused)
 */
static void *att_worker_main(void *arg)
{
	att_worker *worker = arg;
	att_worker_pool *pool = worker->pool;

	while (true) {
		/* Get next test index from queue (mutex-protected) */
		pthread_mutex_lock(&pool->lock);
		if (pool->next_test_index >= pool->total_tests) {
			pthread_mutex_unlock(&pool->lock);
			break; /* All tests have been assigned */
		}
		size_t test_index = pool->next_test_index++;
		pthread_mutex_unlock(&pool->lock);

		/* Get filtered test (no locking needed - registry is frozen) */
		const att_test_case *test = NULL;
		size_t filtered_count = 0;
		for (size_t i = 0; i < pool->registry->count; i++) {
			if (att_filter_match(&pool->registry->tests[i], pool->options)) {
				if (filtered_count == test_index) {
					test = &pool->registry->tests[i];
					break;
				}
				filtered_count++;
			}
		}

		if (!test || !test->fn) {
			continue;
		}

		/* Execute test in thread-local context */
		att_context_begin(test, pool->options->color_enabled, pool->options->format);
		att_context_capture_failures(true); /* Always capture output for parallel execution */

		if (pool->options->timeout_ms > 0) {
			att_context_timeout_start(pool->options->timeout_ms);
		}

		int protect_rc = att_context_protect();
		if (protect_rc == 0) {
			test->fn();
		}

		att_test_result result;
		att_context_end(&result);

		if (pool->options->timeout_ms > 0) {
			att_context_timeout_stop();
		}

		/* Store test reference and result */
		pool->results[test_index].test = test;
		pool->results[test_index].result = result;

		/* Transfer ownership of failure_log to output_buffer */
		if (result.failure_log) {
			pool->results[test_index].output_buffer = result.failure_log;
			pool->results[test_index].output_size = strlen(result.failure_log);
			/* Clear the pointer in result to avoid double-free (ownership transferred) */
			pool->results[test_index].result.failure_log = NULL;
		}

		/* Mark as completed */
		pool->results[test_index].completed = true;
	}

	return NULL;
}

/**
 * Output a single test result in registration order
 *
 * @param result Parallel test result structure
 * @param opts CLI options (for format detection)
 */
static void att_output_parallel_result(const att_parallel_result *result, const att_cli_options *opts)
{
	if (!result || !result->test) {
		return;
	}

	const att_test_case *test = result->test;
	const att_test_result *res = &result->result;
	bool tap_mode = (opts->format == ATT_OUTPUT_TAP);
	bool junit_mode = (opts->format == ATT_OUTPUT_JUNIT);

	/* Suppress default output for TAP/JUnit formats */
	if (tap_mode || junit_mode) {
		return;
	}

	/* Test start message */
	fprintf(stderr, "[ RUN      ] %s.%s\n", test->suite, test->name);

	/* Output captured failure messages */
	if (result->output_buffer && result->output_size > 0) {
		fwrite(result->output_buffer, 1, result->output_size, stderr);
	}

	/* Test end message */
	bool passed = !res->aborted && (res->fail_fatal + res->fail_nonfatal) == 0;
	if (res->skipped) {
		const char *reason = res->skip_reason ? res->skip_reason : "(none)";
		fprintf(stderr, "[  SKIPPED ] %s.%s\n", test->suite, test->name);
		fprintf(stderr, "  reason: %s\n", reason);
	} else if (passed) {
		fprintf(stderr, "[       OK ] %s.%s (%d ms)\n", test->suite, test->name, 0);
	} else {
		fprintf(stderr, "[  FAILED  ] %s.%s\n", test->suite, test->name);
	}
}

/**
 * Run tests in parallel using worker pool
 *
 * @param registry Test registry (must be frozen)
 * @param opts CLI options (must include jobs > 1)
 * @param summary Output summary structure
 * @return 0 on success (all tests passed), 1 on failure
 */
int att_run_tests_parallel(const att_registry *registry, const att_cli_options *opts, att_summary *summary)
{
	if (!registry || !opts || !summary || opts->jobs <= 1) {
		return 3;
	}

	/* Count filtered tests */
	size_t total_tests = 0;
	for (size_t i = 0; i < registry->count; i++) {
		if (att_filter_match(&registry->tests[i], opts)) {
			total_tests++;
		}
	}

	if (total_tests == 0) {
		return 0;
	}

	/* Initialize worker pool */
	att_worker_pool pool;
	if (att_worker_pool_init(&pool, registry, opts, total_tests) != 0) {
		fprintf(stderr, "error: failed to initialize worker pool\n");
		return 3;
	}

	/* Create worker threads */
	size_t worker_count = (size_t)opts->jobs;
	if (worker_count > total_tests) {
		worker_count = total_tests; /* No more workers than tests */
	}

	att_worker *workers = malloc(worker_count * sizeof(att_worker));
	if (!workers) {
		fprintf(stderr, "error: failed to allocate workers\n");
		att_worker_pool_destroy(&pool);
		return 3;
	}

	/* Start worker threads */
	for (size_t i = 0; i < worker_count; i++) {
		workers[i].pool = &pool;
		workers[i].worker_id = (int)i;
		if (pthread_create(&workers[i].thread, NULL, att_worker_main, &workers[i]) != 0) {
			fprintf(stderr, "error: failed to create worker thread %zu\n", i);
			/* Wait for already started threads */
			for (size_t j = 0; j < i; j++) {
				pthread_join(workers[j].thread, NULL);
			}
			free(workers);
			att_worker_pool_destroy(&pool);
			return 3;
		}
	}

	/* Wait for all workers to complete */
	for (size_t i = 0; i < worker_count; i++) {
		pthread_join(workers[i].thread, NULL);
	}

	/* Process results in registration order */
	for (size_t i = 0; i < total_tests; i++) {
		if (!pool.results[i].completed) {
			continue;
		}

		const att_test_result *result = &pool.results[i].result;

		/* Output test result in order */
		att_output_parallel_result(&pool.results[i], opts);

		/* Update summary statistics */
		summary->tests_run++;
		summary->assertions_total += result->assertions_total;

		if (result->skipped) {
			summary->tests_skipped++;
		} else {
			/* Count both fatal and nonfatal failures */
			int test_failures = result->fail_fatal + result->fail_nonfatal;
			summary->failures_total += test_failures;

			if (result->timed_out) {
				summary->timeouts++;
			}

			/* A test is considered failed if it has any failures or was aborted */
			bool failed = (test_failures > 0) || result->aborted || result->timed_out;
			if (failed) {
				summary->tests_failed++;
			}
		}
	}

	/* Cleanup */
	free(workers);
	att_worker_pool_destroy(&pool);

	return summary->tests_failed > 0 ? 1 : 0;
}

#endif /* ATT_THREADS_POSIX */
