/*
 * test_xdr.c
 *
 * Unit tests for the encode/decode (msg2xdr / xdr2msg) roundtrip -- the
 * wire protocol trickle, tricklectl, and trickled use to talk to each
 * other. Regressing this would desync the client/daemon protocol, likely
 * silently (a malformed message that still happens to decode into
 * plausible-looking garbage).
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

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif /* HAVE_SYS_TIME_H */

#include <string.h>

#include "bwstat.h"
#include "message.h"
#include "xdr.h"

#define BUFSZ 2048

static void
roundtrip(struct msg *in, struct msg *out)
{
	u_char buf[BUFSZ];
	uint32_t buflen = BUFSZ;

	assert_int_equal(msg2xdr(in, buf, &buflen), 0);
	assert_true(buflen > 0);
	assert_true(buflen <= BUFSZ);
	assert_int_equal(xdr2msg(out, buf, buflen), 0);
}

static void
test_roundtrip_conf(void **state)
{
	struct msg in, out;

	(void)state;
	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	in.type = MSG_TYPE_CONF;
	in.status = 0;
	in.data.conf.lim[0] = 1024;
	in.data.conf.lim[1] = 2048;
	in.data.conf.pid = 4242;
	in.data.conf.uid = 1000;
	in.data.conf.gid = 1000;
	strlcpy(in.data.conf.argv0, "/usr/bin/curl", sizeof(in.data.conf.argv0));

	roundtrip(&in, &out);

	assert_int_equal(out.type, MSG_TYPE_CONF);
	assert_int_equal(out.data.conf.lim[0], 1024);
	assert_int_equal(out.data.conf.lim[1], 2048);
	assert_int_equal(out.data.conf.pid, 4242);
	assert_int_equal(out.data.conf.uid, 1000);
	assert_int_equal(out.data.conf.gid, 1000);
	assert_string_equal(out.data.conf.argv0, "/usr/bin/curl");
}

static void
test_roundtrip_update(void **state)
{
	struct msg in, out;

	(void)state;
	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	in.type = MSG_TYPE_UPDATE;
	in.status = 0;
	in.data.update.len = 65536;
	in.data.update.dir = BWSTAT_SEND;

	roundtrip(&in, &out);

	assert_int_equal(out.type, MSG_TYPE_UPDATE);
	assert_int_equal(out.data.update.len, 65536);
	assert_int_equal(out.data.update.dir, BWSTAT_SEND);
}

static void
test_roundtrip_delay(void **state)
{
	struct msg in, out;

	(void)state;
	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	in.type = MSG_TYPE_DELAY;
	in.status = 0;
	in.data.delay.len = 65536;
	in.data.delay.dir = 1;

	roundtrip(&in, &out);

	assert_int_equal(out.type, MSG_TYPE_DELAY);
	assert_int_equal(out.data.delay.len, 65536);
	assert_int_equal(out.data.delay.dir, 1);
}

/*
 * msg_update.len/msg_delay.len/msg_delayinfo.len are size_t/ssize_t --
 * 8 bytes on any LP64 host. xdr_msg_{update,delay,delayinfo}() used to
 * marshal them via xdr_int()/xdr_u_int() through an (int *)/(u_int *)
 * cast, which only moves 4 of those 8 bytes: round-tripping any value
 * outside 32-bit int range (negative, or >= 4GB) came back corrupted. Now
 * fixed to marshal through a real int64_t/uint64_t. These three tests
 * pin that fix: each value here is well outside 32-bit range and would
 * have failed under the old (int *)/(u_int *) casts.
 */
static void
test_roundtrip_update_value_beyond_32_bits(void **state)
{
	struct msg in, out;

	(void)state;
	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	in.type = MSG_TYPE_UPDATE;
	in.status = 0;
	in.data.update.len = 5000000000ULL; /* > UINT32_MAX */
	in.data.update.dir = BWSTAT_RECV;

	roundtrip(&in, &out);

	assert_int_equal(out.data.update.len, 5000000000ULL);
}

static void
test_roundtrip_delay_value_beyond_32_bits(void **state)
{
	struct msg in, out;

	(void)state;
	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	in.type = MSG_TYPE_DELAY;
	in.status = 0;
	in.data.delay.len = 5000000000LL; /* > INT32_MAX */
	in.data.delay.dir = 0;

	roundtrip(&in, &out);

	assert_int_equal(out.data.delay.len, 5000000000LL);
}

static void
test_roundtrip_delay_negative_value(void **state)
{
	struct msg in, out;

	(void)state;
	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	/*
	 * No current caller sends a negative msg_delay.len, but the field
	 * is declared ssize_t, so the wire format should still preserve
	 * sign correctly for any value the type allows.
	 */
	in.type = MSG_TYPE_DELAY;
	in.status = 0;
	in.data.delay.len = -1;
	in.data.delay.dir = 0;

	roundtrip(&in, &out);

	assert_int_equal(out.data.delay.len, -1);
}

static void
test_roundtrip_delayinfo(void **state)
{
	struct msg in, out;

	(void)state;
	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	in.type = MSG_TYPE_DELAYINFO;
	in.status = 0;
	in.data.delayinfo.delaytv.tv_sec = 3;
	in.data.delayinfo.delaytv.tv_usec = 500000;
	in.data.delayinfo.len = 12345;

	roundtrip(&in, &out);

	assert_int_equal(out.type, MSG_TYPE_DELAYINFO);
	assert_int_equal(out.data.delayinfo.delaytv.tv_sec, 3);
	assert_int_equal(out.data.delayinfo.delaytv.tv_usec, 500000);
	assert_int_equal(out.data.delayinfo.len, 12345);
}

static void
test_roundtrip_delayinfo_value_beyond_32_bits(void **state)
{
	struct msg in, out;

	(void)state;
	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	in.type = MSG_TYPE_DELAYINFO;
	in.status = 0;
	in.data.delayinfo.delaytv.tv_sec = 1;
	in.data.delayinfo.delaytv.tv_usec = 0;
	in.data.delayinfo.len = 5000000000LL; /* > INT32_MAX */

	roundtrip(&in, &out);

	assert_int_equal(out.data.delayinfo.len, 5000000000LL);
}

static void
test_roundtrip_getinfo(void **state)
{
	struct msg in, out;

	(void)state;
	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	in.type = MSG_TYPE_GETINFO;
	in.status = 0;
	in.data.getinfo.dirinfo[0].lim = 100;
	in.data.getinfo.dirinfo[0].rate = 50;
	in.data.getinfo.dirinfo[1].lim = 200;
	in.data.getinfo.dirinfo[1].rate = 150;

	roundtrip(&in, &out);

	assert_int_equal(out.type, MSG_TYPE_GETINFO);
	assert_int_equal(out.data.getinfo.dirinfo[0].lim, 100);
	assert_int_equal(out.data.getinfo.dirinfo[0].rate, 50);
	assert_int_equal(out.data.getinfo.dirinfo[1].lim, 200);
	assert_int_equal(out.data.getinfo.dirinfo[1].rate, 150);
}

static void
test_roundtrip_preserves_status(void **state)
{
	struct msg in, out;

	(void)state;
	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	in.type = MSG_TYPE_UPDATE;
	in.status = MSG_STATUS_FAIL;
	in.data.update.len = 1;
	in.data.update.dir = 0;

	roundtrip(&in, &out);

	assert_int_equal(out.status, MSG_STATUS_FAIL);
}

int
main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_roundtrip_conf),
		cmocka_unit_test(test_roundtrip_update),
		cmocka_unit_test(test_roundtrip_delay),
		cmocka_unit_test(test_roundtrip_update_value_beyond_32_bits),
		cmocka_unit_test(test_roundtrip_delay_value_beyond_32_bits),
		cmocka_unit_test(test_roundtrip_delay_negative_value),
		cmocka_unit_test(test_roundtrip_delayinfo),
		cmocka_unit_test(test_roundtrip_delayinfo_value_beyond_32_bits),
		cmocka_unit_test(test_roundtrip_getinfo),
		cmocka_unit_test(test_roundtrip_preserves_status),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
