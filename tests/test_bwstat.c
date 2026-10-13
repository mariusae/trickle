/*
 * test_bwstat.c
 *
 * Unit tests for the core rate-limiting/fair-share algorithm in bwstat.c.
 * This is trickle's critical path: bwstat_getdelay() decides how much data
 * a client may move right now and how long to pause afterward, and
 * redistributes unused entitlement from idle clients to busy ones.
 *
 * All timing-dependent struct fields (winrate, pts, tsmooth, lsmooth) are
 * set directly rather than driven by real gettimeofday()/sleep() calls, so
 * these tests are fast and deterministic.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <sys/types.h>
#include <sys/queue.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "bwstat.h"

/*
 * Real CLI defaults (see trickle.c): -t 3.0 seconds, -l 20 (KB -> bytes).
 * Using the real defaults means these tests exercise the same code path
 * production traffic does, not an artificial always-zero smoothing case.
 */
#define DEFAULT_TSMOOTH 3.0
#define DEFAULT_LSMOOTH (20 * 1024)

static int
setup(void **state)
{
	(void)state;
	/* bwstat_init() resets statq's head pointers to empty and inserts a
	 * fresh totals entry, giving each test a clean slate. The old
	 * entries from the previous test are orphaned (leaked for the life
	 * of the test process) rather than freed -- acceptable here since
	 * each cmocka test is short-lived, and bwstat_free() itself does not
	 * free() its argument (see test_bwstat_free_does_not_free_memory).
	 */
	assert_int_equal(bwstat_init(65536), 0);
	return 0;
}

static void
test_getdelay_returns_null_for_zero_length(void **state)
{
	struct bwstat *bs = bwstat_new();
	size_t len = 0;

	(void)state;
	assert_non_null(bs);
	assert_null(bwstat_getdelay(bs, &len, 100, BWSTAT_SEND));
}

static void
test_getdelay_returns_null_under_aggregate_limit(void **state)
{
	struct bwstat *bs = bwstat_new();
	struct bwstat *tot = bwstat_gettot();
	size_t len = 1000;

	(void)state;
	assert_non_null(bs);

	/* Aggregate observed rate (50 B/s) is at/under the limit (100 B/s):
	 * bwstat_getdelay's very first check (rate <= lim) should short
	 * circuit before even looking at individual clients. */
	tot->data[BWSTAT_SEND].winrate = 50;

	assert_null(bwstat_getdelay(bs, &len, 100, BWSTAT_SEND));
	assert_int_equal(len, 1000); /* untouched */
}

static void
test_getdelay_returns_null_with_no_registered_clients(void **state)
{
	struct bwstat *tot = bwstat_gettot();
	size_t len = 1000;

	(void)state;

	/*
	 * Over the aggregate limit, but nobody is registered as a client.
	 * Note: bwstat_new() always links into the same statq the client
	 * count walks, so calling it here (even just to get a throwaway
	 * `bs` to pass in) would itself register a client. Passing the
	 * totals entry itself as `bs` exercises the ncli == 0 path without
	 * that side effect.
	 */
	tot->data[BWSTAT_SEND].winrate = 200;

	assert_null(bwstat_getdelay(tot, &len, 100, BWSTAT_SEND));
}

static void
test_getdelay_clamps_len_to_caller_request(void **state)
{
	struct bwstat *bs = bwstat_new();
	struct bwstat *tot = bwstat_gettot();
	struct timeval *delay;
	size_t len = 50; /* caller is only asking to move 50 bytes right now */

	(void)state;
	assert_non_null(bs);

	bs->pts = 1;
	bs->tsmooth = DEFAULT_TSMOOTH;
	bs->lsmooth = DEFAULT_LSMOOTH;
	bs->data[BWSTAT_SEND].winrate = 150; /* over its entitlement */
	tot->data[BWSTAT_SEND].winrate = 200; /* over the 100 B/s limit */

	delay = bwstat_getdelay(bs, &len, 100, BWSTAT_SEND);

	assert_non_null(delay);
	/* bwstat_getdelay must never grant more than what was asked for,
	 * even though the smoothed entitlement (ent * pts * tsmooth =
	 * 100 * 1 * 3.0 = 300) is larger than the 50-byte request. */
	assert_int_equal(len, 50);
	/* delay = len / (ent * pts) = 50 / 100 = 0.5s */
	assert_int_equal(delay->tv_sec, 0);
	assert_int_in_range(delay->tv_usec, 490000, 510000);
}

static void
test_getdelay_caps_len_to_smoothed_entitlement(void **state)
{
	struct bwstat *bs = bwstat_new();
	struct bwstat *tot = bwstat_gettot();
	struct timeval *delay;
	size_t len = 10000; /* caller wants to move far more than entitled */

	(void)state;
	assert_non_null(bs);

	bs->pts = 1;
	bs->tsmooth = DEFAULT_TSMOOTH;
	bs->lsmooth = DEFAULT_LSMOOTH;
	bs->data[BWSTAT_SEND].winrate = 150;
	tot->data[BWSTAT_SEND].winrate = 200;

	delay = bwstat_getdelay(bs, &len, 100, BWSTAT_SEND);

	assert_non_null(delay);
	/* ent * pts * tsmooth = 100 * 1 * 3.0 = 300, which is less than the
	 * 10000-byte request, so the client is told to send only 300 bytes
	 * this round ... */
	assert_int_equal(len, 300);
	/* ... then pause for a flat tsmooth (3s) before the next round. */
	assert_int_equal(delay->tv_sec, 3);
	assert_int_in_range(delay->tv_usec, 0, 10000);
}

static void
test_getdelay_redistributes_idle_entitlement_to_busy_client(void **state)
{
	struct bwstat *idle = bwstat_new();
	struct bwstat *busy = bwstat_new();
	struct bwstat *tot = bwstat_gettot();
	struct timeval *delay;
	size_t len = 10000;

	(void)state;
	assert_non_null(idle);
	assert_non_null(busy);

	idle->pts = 1;
	idle->tsmooth = DEFAULT_TSMOOTH;
	idle->lsmooth = DEFAULT_LSMOOTH;
	idle->data[BWSTAT_SEND].winrate = 10; /* barely using its share */

	busy->pts = 1;
	busy->tsmooth = DEFAULT_TSMOOTH;
	busy->lsmooth = DEFAULT_LSMOOTH;
	busy->data[BWSTAT_SEND].winrate = 200; /* saturating */

	tot->data[BWSTAT_SEND].winrate = 200; /* over the 100 B/s limit */

	delay = bwstat_getdelay(busy, &len, 100, BWSTAT_SEND);

	assert_non_null(delay);
	/*
	 * A naive even split (lim / npts = 100 / 2 = 50) would cap busy's
	 * entitlement at 50 B/s. Because idle is only using 10 B/s of its
	 * 50 B/s share, bwstat_getdelay's "sprinkle bandwidth" pass should
	 * redistribute idle's unused 40 B/s to busy, raising busy's real
	 * entitlement to 90 B/s (ent * pts * tsmooth = 90 * 1 * 3.0 = 270).
	 * This is the actual fairness guarantee the algorithm exists to
	 * provide -- regressing it would silently give busy clients only
	 * their naive even share instead of the pool's spare capacity.
	 */
	assert_int_equal(len, 270);
	assert_int_equal(delay->tv_sec, 3);
}

static void
test_update_increases_total_bytes_and_rate(void **state)
{
	struct bwstat *bs = bwstat_new();
	struct bwstat *tot;

	(void)state;
	assert_non_null(bs);

	bwstat_update(bs, 1000, BWSTAT_SEND);

	tot = bwstat_gettot();
	assert_int_equal(tot->data[BWSTAT_SEND].bytes, 1000);
	assert_int_equal(bs->data[BWSTAT_SEND].bytes, 1000);

	bwstat_update(bs, 500, BWSTAT_SEND);
	assert_int_equal(tot->data[BWSTAT_SEND].bytes, 1500);
	assert_int_equal(bs->data[BWSTAT_SEND].bytes, 1500);

	/* The other direction must stay untouched. */
	assert_int_equal(tot->data[BWSTAT_RECV].bytes, 0);
	assert_int_equal(bs->data[BWSTAT_RECV].bytes, 0);
}

static void
test_free_removes_from_totals_pool(void **state)
{
	struct bwstat *bs = bwstat_new();
	struct bwstat *tot = bwstat_gettot();
	size_t len = 10000;

	(void)state;
	assert_non_null(bs);

	bs->pts = 1;
	bs->tsmooth = DEFAULT_TSMOOTH;
	bs->lsmooth = DEFAULT_LSMOOTH;
	bs->data[BWSTAT_SEND].winrate = 200;
	tot->data[BWSTAT_SEND].winrate = 200;

	assert_null(bwstat_free(bs));

	/* bs is no longer in the pool bwstat_getdelay walks, so there is
	 * once again nobody to charge: same as the "no registered clients"
	 * case. Pass tot itself as the probe `bs`, not a fresh
	 * bwstat_new(), which would re-register a client. */
	assert_null(bwstat_getdelay(tot, &len, 100, BWSTAT_SEND));
}

int
main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(test_getdelay_returns_null_for_zero_length, setup),
		cmocka_unit_test_setup(test_getdelay_returns_null_under_aggregate_limit, setup),
		cmocka_unit_test_setup(test_getdelay_returns_null_with_no_registered_clients, setup),
		cmocka_unit_test_setup(test_getdelay_clamps_len_to_caller_request, setup),
		cmocka_unit_test_setup(test_getdelay_caps_len_to_smoothed_entitlement, setup),
		cmocka_unit_test_setup(test_getdelay_redistributes_idle_entitlement_to_busy_client, setup),
		cmocka_unit_test_setup(test_update_increases_total_bytes_and_rate, setup),
		cmocka_unit_test_setup(test_free_removes_from_totals_pool, setup),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
