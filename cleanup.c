/*
 * cleanup.c
 *
 * Copyright (c) 2002 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id: cleanup.c,v 1.2 2003/03/06 05:49:36 marius Exp $
 */

#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/queue.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

struct cleanupq {
	void                   (*f)(void *);
	void                  *handler;
	TAILQ_ENTRY(cleanupq)  next;
};

TAILQ_HEAD(cleanupqh, cleanupq);

struct cleanup {
	struct cleanupqh head;
};

struct cleanup *
cleanup_new(void)
{
	struct cleanup *clup;

	if ((clup = calloc(1, sizeof(*clup))) == NULL)
		return (NULL);

	TAILQ_INIT(&clup->head);

	return (clup);
}

struct cleanup *
cleanup_free(struct cleanup *clup)
{
	struct cleanupq *cq;

	while ((cq = TAILQ_FIRST(&clup->head)) != NULL) {
		TAILQ_REMOVE(&clup->head, cq, next);
		free(cq);
	}

	free(clup);

	return (NULL);
}

int
cleanup_add(struct cleanup *clup, void (*f)(void *), void *handler)
{
	struct cleanupq *cq;

	if ((cq = malloc(sizeof(*cq))) == NULL)
		return (-1);

	cq->f = f;
	cq->handler = handler;

	TAILQ_INSERT_HEAD(&clup->head, cq, next);

	return (0);
}

int
cleanup_remove(struct cleanup *clup, void (*f)(void *), void *handler)
{
	struct cleanupq *search;

	TAILQ_FOREACH(search, &clup->head, next)
		if (search->f == f && search->handler == handler)
			break;

	if (search != NULL) {
		TAILQ_REMOVE(&clup->head, search, next);
		free(search);
		return (0);
	}

	return (-1);
}

void
cleanup_cleanup(struct cleanup *clup)
{
	struct cleanupq *cq;

	while ((cq = TAILQ_FIRST(&clup->head)) != NULL) {
		(*cq->f)(cq->handler);
		TAILQ_REMOVE(&clup->head, cq, next);
		free(cq);
	}
}

void
cleanupcb_close(void *_fd)
{
	int fd = *(int *)_fd;

	close(fd);
}

void
cleanupcb_unlink(void *_fname)
{
	char *fname = (char *)_fname;

	unlink(fname);
}
