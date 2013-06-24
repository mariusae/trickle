/*
 * trickledu.h
 *
 * Copyright (c) 2003 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id: trickledu.h,v 1.7 2004/02/13 06:11:21 marius Exp $
 */

#ifndef TRICKLE_TRICKLEDU_H
#define TRICKLE_TRICKLEDU_H

void            trickled_configure(char *, int (*)(int, int, int),
                    ssize_t (*)(int, void *, size_t),
                    ssize_t (*)(int, const void *, size_t),
		    int (*)(int), char *);
void            trickled_open(int *);
int             trickled_update(short, size_t);
int             trickled_delay(short, size_t *);
struct timeval *trickled_getdelay(short, size_t *);
int             trickled_sendmsg(struct msg *);
int             trickled_recvmsg(struct msg *);
void            trickled_ctl_open(int *);
int             trickled_getinfo(uint32_t *, uint32_t *,
                    uint32_t *, uint32_t *);
void            trickled_close(int *);

#endif /* TRICKLE_TRICKLEDU_H */
