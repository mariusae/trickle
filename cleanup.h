/*
 * cleanup.h
 *
 * Copyright (c) 2002 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id: cleanup.h,v 1.1 2003/03/06 04:11:39 marius Exp $
 */

#ifndef TRICKLE_CLEANUP_H
#define TRICKLE_CLEANUP_H

typedef struct cleanup cleanup_t;

cleanup_t *cleanup_new(void);
cleanup_t *cleanup_free(cleanup_t *);
int        cleanup_add(cleanup_t *, void (*)(void *), void *);
int        cleanup_remove(cleanup_t *, void (*)(void *), void *);
void       cleanup_cleanup(cleanup_t *);

/* Utility */
void       cleanupcb_close(void *);
void       cleanupcb_unlink(void *);

#endif /* TRICKLE_CLEANUP_H */
