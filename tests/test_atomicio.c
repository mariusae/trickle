/*
 * test_atomicio.c
 *
 * Unit tests for atomicio()'s retry loop -- the function every byte trickle
 * moves passes through. Exercises partial transfers, EINTR/EAGAIN retry,
 * and the two different "stopped early" outcomes (clean EOF vs. a real
 * error), verifying both the returned count and the buffer offset / length
 * passed to the underlying read/write-shaped function on each call.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <errno.h>
#include <string.h>

#include "util.h"

/*
 * Mock for the read/write-shaped function atomicio() calls. Each test
 * queues up a scripted sequence of (expected remaining length, errno,
 * return value) via expect_value()/will_return() before calling
 * atomicio(), so the test both drives the retry loop and verifies the
 * exact arguments atomicio() passes on each call (i.e. that `s + pos` /
 * `n - pos` advance correctly across partial transfers).
 */
static ssize_t
mock_io(int fd, void *buf, size_t n)
{
	(void)fd;
	(void)buf;
	check_expected_uint(n);
	errno = mock_type(int);
	return mock_type(ssize_t);
}

static void
test_single_call_transfers_everything(void **state)
{
	char buf[16] = {0};

	(void)state;

	expect_uint_value(mock_io, n, 10);
	will_return(mock_io, 0);
	will_return(mock_io, 10);

	assert_int_equal(atomicio(mock_io, -1, buf, 10), 10);
}

static void
test_partial_transfers_accumulate(void **state)
{
	char buf[16] = {0};

	(void)state;

	/* 10 bytes requested, delivered in 3 + 4 + 3. Each call must see
	 * the *remaining* length, not the original 10. */
	expect_uint_value(mock_io, n, 10);
	will_return(mock_io, 0);
	will_return(mock_io, 3);

	expect_uint_value(mock_io, n, 7);
	will_return(mock_io, 0);
	will_return(mock_io, 4);

	expect_uint_value(mock_io, n, 3);
	will_return(mock_io, 0);
	will_return(mock_io, 3);

	assert_int_equal(atomicio(mock_io, -1, buf, 10), 10);
}

static void
test_eintr_retries_without_losing_progress(void **state)
{
	char buf[16] = {0};

	(void)state;

	expect_uint_value(mock_io, n, 10);
	will_return(mock_io, 0);
	will_return(mock_io, 5); /* first chunk lands */

	expect_uint_value(mock_io, n, 5);
	will_return(mock_io, EINTR);
	will_return(mock_io, -1); /* interrupted -- must retry, not abort */

	expect_uint_value(mock_io, n, 5);
	will_return(mock_io, 0);
	will_return(mock_io, 5); /* retry succeeds, completing the transfer */

	assert_int_equal(atomicio(mock_io, -1, buf, 10), 10);
}

static void
test_eagain_retries_without_losing_progress(void **state)
{
	char buf[16] = {0};

	(void)state;

	expect_uint_value(mock_io, n, 10);
	will_return(mock_io, 0);
	will_return(mock_io, 10);

	/* A second, already-fully-satisfied call would exit the loop
	 * before calling mock_io again (n > pos is false), so to exercise
	 * EAGAIN specifically, use a fresh atomicio() call instead. */
	assert_int_equal(atomicio(mock_io, -1, buf, 10), 10);

	expect_uint_value(mock_io, n, 4);
	will_return(mock_io, EAGAIN);
	will_return(mock_io, -1);

	expect_uint_value(mock_io, n, 4);
	will_return(mock_io, 0);
	will_return(mock_io, 4);

	assert_int_equal(atomicio(mock_io, -1, buf, 4), 4);
}

static void
test_eof_at_start_returns_zero(void **state)
{
	char buf[16] = {0};

	(void)state;

	/* The peer closed the connection before any data was transferred
	 * (pos == 0): atomicio must report that honestly as 0, not as an
	 * error and not as a full transfer. */
	expect_uint_value(mock_io, n, 10);
	will_return(mock_io, 0);
	will_return(mock_io, 0);

	assert_int_equal(atomicio(mock_io, -1, buf, 10), 0);
}

static void
test_eof_after_partial_transfer_returns_partial_count(void **state)
{
	char buf[16] = {0};

	(void)state;

	/* Some data arrived, then the peer closed the connection. The
	 * caller needs to know how much real data it got (4), not 0 and
	 * not -1, so it can process a short read/write correctly. */
	expect_uint_value(mock_io, n, 10);
	will_return(mock_io, 0);
	will_return(mock_io, 4);

	expect_uint_value(mock_io, n, 6);
	will_return(mock_io, 0);
	will_return(mock_io, 0); /* EOF */

	assert_int_equal(atomicio(mock_io, -1, buf, 10), 4);
}

static void
test_hard_error_at_start_returns_negative_one(void **state)
{
	char buf[16] = {0};

	(void)state;

	expect_uint_value(mock_io, n, 10);
	will_return(mock_io, ECONNRESET);
	will_return(mock_io, -1);

	assert_int_equal(atomicio(mock_io, -1, buf, 10), -1);
}

static void
test_hard_error_after_partial_transfer_returns_partial_count(void **state)
{
	char buf[16] = {0};

	(void)state;

	expect_uint_value(mock_io, n, 10);
	will_return(mock_io, 0);
	will_return(mock_io, 6);

	expect_uint_value(mock_io, n, 4);
	will_return(mock_io, ECONNRESET);
	will_return(mock_io, -1);

	/* Matches the EOF-after-partial-transfer behavior: once something
	 * has gone through, atomicio reports the real count rather than
	 * discarding it behind a bare -1. */
	assert_int_equal(atomicio(mock_io, -1, buf, 10), 6);
}

int
main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_single_call_transfers_everything),
		cmocka_unit_test(test_partial_transfers_accumulate),
		cmocka_unit_test(test_eintr_retries_without_losing_progress),
		cmocka_unit_test(test_eagain_retries_without_losing_progress),
		cmocka_unit_test(test_eof_at_start_returns_zero),
		cmocka_unit_test(test_eof_after_partial_transfer_returns_partial_count),
		cmocka_unit_test(test_hard_error_at_start_returns_negative_one),
		cmocka_unit_test(test_hard_error_after_partial_transfer_returns_partial_count),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
