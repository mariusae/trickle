/*
 * bwstat.h
 *
 * Copyright (c) 2003 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id: bwstat.h,v 1.5 2003/04/06 00:29:37 marius Exp $
 */

#ifndef TRICKLE_BWSTAT
#define TRICKLE_BWSTAT

#define BWSTAT_SEND 0
#define BWSTAT_RECV 1

struct bwstat_data {
	uint32_t            bytes;
	uint32_t            rate;
	struct timeval      tv;

	uint32_t            winbytes;
	uint32_t            winrate;
	struct timeval      wintv;
};

struct bwstat {
	struct bwstat_data  data[2];
	uint                pts;
	uint                lsmooth;
	double              tsmooth;
	TAILQ_ENTRY(bwstat) next, qnext;
};

int             bwstat_init(uint);
struct bwstat  *bwstat_new(void);
struct bwstat  *bwstat_free(struct bwstat *);
void            bwstat_update(struct bwstat *, size_t, short);
struct timeval *bwstat_getdelay(struct bwstat *, size_t *, uint, short);
struct bwstat  *bwstat_gettot(void);

#endif /* TRICKLE_BWSTAT */
