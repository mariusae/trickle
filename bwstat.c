/*
 * bwstat.c
 *
 * Copyright (c) 2003 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id: bwstat.c,v 1.15 2003/07/29 05:58:58 marius Exp $ 
 */

#include <sys/types.h>
#include <sys/queue.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif /* HAVE_SYS_TIME_H */

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#if defined(HAVE_TIME_H) && defined(TIME_WITH_SYS_TIME)
#include <time.h>
#endif /* defined(HAVE_TIME_H) && defined(TIME_WITH_SYS_TIME) */
#include <string.h>

#include "util.h"
#include "bwstat.h"

static TAILQ_HEAD(bwstathead, bwstat) statq;
static uint winsz;

static double difftv(struct timeval *, struct timeval *);
static void   _bwstat_update(struct bwstat_data *, size_t);

#define INITTV(tv, curtv) do {			\
	if (!timerisset(&(tv)))			\
		(tv) = (curtv);			\
} while (0)

/*
 * XXX should winsz be per-update?
 */
int
bwstat_init(uint xwinsz)
{
	struct bwstat *bs;

	winsz = xwinsz;

	TAILQ_INIT(&statq);

	/* First entry is the totals. */
	if ((bs = bwstat_new()) == NULL)
		return (-1);

	return (0);
}

struct bwstat *
bwstat_new(void)
{
	struct bwstat *bs;

	if ((bs = calloc(1, sizeof(*bs))) == NULL)
		return (NULL);

	TAILQ_INSERT_TAIL(&statq, bs, next);

	return (bs);
}

struct bwstat *
bwstat_free(struct bwstat *bs)
{
	TAILQ_REMOVE(&statq, bs, next);

	return (NULL);
}

struct bwstat *
bwstat_gettot(void)
{
	return (TAILQ_FIRST(&statq));
}

void
bwstat_update(struct bwstat *bs, size_t len, short which)
{
	struct bwstat *bstot = TAILQ_FIRST(&statq);

	if (bs != NULL)
		_bwstat_update(&bs->data[which], len);
	_bwstat_update(&bstot->data[which], len);
}

static void
_bwstat_update(struct bwstat_data *bsd, size_t len)
{
	struct timeval curtv;
	double elap, elapwin;

	gettimeofday(&curtv, NULL);

	INITTV(bsd->tv, curtv);
	INITTV(bsd->wintv, curtv);

	elap = difftv(&curtv, &bsd->tv);
	elapwin = difftv(&curtv, &bsd->wintv);

	/* XXX Window follows. */
	if (bsd->winbytes == 0 && bsd->winrate > 0)
		bsd->winbytes = (1.0 * bsd->winrate) * elapwin;

	bsd->bytes += len;
	bsd->winbytes += len;

	if (elap == 0.0 || elapwin == 0.0)
		return;

	bsd->rate = (1.0 * bsd->bytes) / elap;
	bsd->winrate = (1.0 * bsd->winbytes) / elapwin;

	/*
	 * Reset window; XXX make this a sliding window wrt a
	 * timeline; keep average, but reduce #bytes?
	 */

	if (bsd->winbytes >= winsz) {
		gettimeofday(&bsd->wintv, NULL);
		bsd->winbytes = 0;
	}
}

/*
 * Return required delay for bs for direction which.
 */

struct timeval *
bwstat_getdelay(struct bwstat *bs, size_t *len, uint lim, short which)
{
	uint rate = 0, ncli = 0, npts = 0, pool = 0, ent, xent;
	double delay;
	static struct timeval tv;
	struct bwstathead poolq;
	struct bwstat *xbs, *bstot = TAILQ_FIRST(&statq);
	uint initent;
	size_t xlen = *len;

	if (*len == 0)
		return (NULL);

	memset(&tv, 0, sizeof(tv));

	TAILQ_INIT(&poolq);

	rate = bstot->data[which].winrate;

	if (rate <= lim)
		return (NULL);

	xbs = bstot;
	while ((xbs = TAILQ_NEXT(xbs, next)) != NULL) {
		ncli++;
		npts += xbs->pts;
		TAILQ_INSERT_TAIL(&poolq, xbs, qnext);
	}

	if (ncli == 0)
		return (NULL);

	/* Entitlement per point */
	initent = ent = lim / npts;

	if (ent == 0)
		;		/*
				 * XXX we have surpassed our
				 * granularity.
				 */

	/*
	 * Sprinkle some bandwidth: increase the value of a point
	 * until everyone is satisfied.
	 */
	do {
		/* Take from the poor ... */
		TAILQ_FOREACH(xbs, &poolq, qnext)
			if (xbs->data[which].winrate < ent * xbs->pts) {
				pool += ent * xbs->pts -
				    xbs->data[which].winrate;
				ncli--;
				npts -= xbs->pts;
				TAILQ_REMOVE(&poolq, xbs, qnext);
			}

		/* And give to the rich ... */
		if (ncli > 0) {
			xent = pool / npts;

			if (xent == 0)
				break;

			TAILQ_FOREACH(xbs, &poolq, qnext)
				if (xbs->data[which].winrate > ent * xbs->pts)
					pool -= xent * xbs->pts;

			ent += xent;
		}
	} while (pool > 0 && ncli > 0);

	/*
	 * This is the case of a client that is not using its limit.
	 * We reset ent in this case.  The rest will adjust itself
	 * over time.
	 */
	if (ent * bs->pts > lim)
		ent = lim / bs->pts;

	if (bs->data[which].winrate > ent * bs->pts)
		delay = (1.0 * *len) / (1.0 * ent * bs->pts);
	else
		delay = 0.0;

/*	if (delay > bs->tsmooth) {  */
		if ((*len = ent * bs->pts * bs->tsmooth) == 0) {
			*len = bs->lsmooth;
			delay = (1.0 * *len) / (1.0 * ent * bs->pts);
		} else
			delay = bs->tsmooth;
/*	} */

	if (*len > xlen) {
		*len = xlen;
		delay = (1.0 * *len) / (1.0 * ent * bs->pts);
	}

	/* XXX */
	if (delay < 0.0)
		return (NULL);

	tv.tv_sec = delay;
	tv.tv_usec = (delay - tv.tv_sec) * 1000000L;

	return (&tv);
}

static double
difftv(struct timeval *tv0, struct timeval *tv1)
{
	struct timeval diff_tv;

	timersub(tv0, tv1, &diff_tv);
        return (diff_tv.tv_sec + (diff_tv.tv_usec / 1000000.0));
}
