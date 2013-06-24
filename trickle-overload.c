/*
 * trickle-overload.c
 *
 * Copyright (c) 2002, 2003 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id: trickle-overload.c,v 1.38 2004/02/13 06:11:21 marius Exp $
 */

/* Ick.  linux sucks. */
#define _GNU_SOURCE

#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/un.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif /* HAVE_SYS_TIME_H */

#include <netinet/in.h>

#ifdef HAVE_ERR_H
#include <err.h>
#endif /* HAVE_ERR_H */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <limits.h>
#include <math.h>
#if defined(HAVE_TIME_H) && defined(TIME_WITH_SYS_TIME)
#include <time.h>
#endif /* defined(HAVE_TIME_H) && defined(TIME_WITH_SYS_TIME) */
#include <syslog.h>
#include <pwd.h>
#include <stdarg.h>
#include <string.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */

#include "bwstat.h"
#include "trickle.h"
#include "message.h"
#include "util.h"
#include "trickledu.h"

#ifndef INFTIM
#define INFTIM -1
#endif /* INFTIM */

#define SD_INSELECT 0x01

struct sockdesc {
	int                    sock;
	int                    flags;
	struct bwstat         *stat;
	struct {
		int     flags;
		size_t  lastlen;
		size_t  selectlen;
	}                      data[2];

	TAILQ_ENTRY(sockdesc)  next;
};

struct delay {
	struct sockdesc    *sd;
	struct timeval      delaytv;
	struct timeval      abstv;
	short               which;
	short               pollevents;
	int                 pollidx;

	TAILQ_ENTRY(delay)  next;	
};

TAILQ_HEAD(delayhead, delay);

struct _pollfd {
	struct pollfd        *pfd;
	int                   idx;

	TAILQ_ENTRY(_pollfd)  next;
};

TAILQ_HEAD(_pollfdhead, _pollfd);

static TAILQ_HEAD(sockdeschead, sockdesc) sdhead;
static uint32_t winsz;
static int verbose;
static uint lim[2];
static char *argv0;
static double tsmooth;
static uint lsmooth/* , latency */;
static int trickled, initialized, initializing;
/* XXX initializing - volatile? */

#define DECLARE(name, ret, args) static ret (*libc_##name) args

DECLARE(socket, int, (int, int, int));
DECLARE(close, int, (int));
/* DECLARE(setsockopt, int, (int, int, int, const void *, socklen_t)); */

DECLARE(read, ssize_t, (int, void *, size_t));
DECLARE(recv, ssize_t, (int, void *, size_t, int));
DECLARE(readv, ssize_t, (int, const struct iovec *, int));
#ifdef __sun__
DECLARE(recvfrom, ssize_t, (int, void *, size_t, int, struct sockaddr *,
	    Psocklen_t));
#else
DECLARE(recvfrom, ssize_t, (int, void *, size_t, int, struct sockaddr *,
	    socklen_t *));
#endif /* __sun__ */

DECLARE(write, ssize_t, (int, const void *, size_t));
DECLARE(send, ssize_t, (int, const void *, size_t, int));
DECLARE(writev, ssize_t, (int, const struct iovec *, int));
DECLARE(sendto, ssize_t, (int, const void *, size_t, int,
	    const struct sockaddr *, socklen_t));

DECLARE(select, int, (int, fd_set *, fd_set *, fd_set *, struct timeval *));
DECLARE(poll, int, (struct pollfd *, int, int));

#ifdef __sun__
DECLARE(accept, int, (int, struct sockaddr *, Psocklen_t));
#else
DECLARE(accept, int, (int, struct sockaddr *, socklen_t *));
#endif /* __sun__ */
DECLARE(dup, int, (int));
DECLARE(dup2, int, (int, int));

#ifdef HAVE_SENDFILE
DECLARE(sendfile, ssize_t, (int, int, off_t *, size_t));
#endif

static int             delay(int, ssize_t *, short);
static struct timeval *getdelay(struct sockdesc *, ssize_t *, short);
static void            update(int, ssize_t, short);
static void            updatesd(struct sockdesc *, ssize_t, short);
static void            trickle_init(void);
void                   safe_printv(int, const char *, ...);

#define errx(l, fmt, arg...) do {		\
	safe_printv(0, fmt, ##arg);		\
	exit(l);				\
} while (0)

#ifdef DL_NEED_UNDERSCORE
#define UNDERSCORE "_"
#else
#define UNDERSCORE ""
#endif /* DL_NEED_UNDERSCORE */

#define INIT do {				\
	if (!initialized && !initializing)	\
		trickle_init();			\
} while (0)

#define GETADDR(x) do {							\
	if ((libc_##x = dlsym(dh, UNDERSCORE #x)) == NULL)		\
		errx(0, "[trickle] Failed to get " #x "() address");	\
} while (0)

static void
trickle_init(void)
{
	void *dh;
	char *winszstr, *verbosestr,
	    *recvlimstr, *sendlimstr, *sockname, *tsmoothstr, *lsmoothstr;
/* 	    *latencystr; */

	initializing = 1;

#ifdef NODLOPEN
	dh = (void *) -1L;
#else
 	if ((dh = dlopen(DLOPENLIBC, RTLD_LAZY)) == NULL)
		errx(1, "[trickle] Failed to open libc");
#endif /* DLOPEN */

	/*
	 * We get write first, so that we have a bigger chance of
	 * exiting gracefully with safe_printv.
	 */

	GETADDR(write);

	GETADDR(socket);
/*	GETADDR(setsockopt); */
	GETADDR(close);

	GETADDR(read);
	GETADDR(readv);
#ifndef __FreeBSD__
	GETADDR(recv);
#endif /* !__FreeBSD__ */
	GETADDR(recvfrom);

	GETADDR(writev);
#ifndef __FreeBSD__
	GETADDR(send);
#endif /* !__FreeBSD__ */
	GETADDR(sendto);

	GETADDR(select);
//	GETADDR(poll);

	GETADDR(dup);
	GETADDR(dup2);

	GETADDR(accept);

#ifdef HAVE_SENDFILE
	GETADDR(sendfile);
#endif

	/* XXX pthread test */
/*  	if ((dh = dlopen("/usr/lib/libpthread.so.1.0", RTLD_LAZY)) == NULL) */
/* 		errx(1, "[trickle] Failed to open libpthread"); */

	GETADDR(poll);

	if ((winszstr = getenv("TRICKLE_WINDOW_SIZE")) == NULL)
		errx(1, "[trickle] Failed to get window size");

	if ((recvlimstr = getenv("TRICKLE_DOWNLOAD_LIMIT")) == NULL)
		errx(1, "[trickle] Failed to get limit");

	if ((sendlimstr = getenv("TRICKLE_UPLOAD_LIMIT")) == NULL)
		errx(1, "[trickle] Failed to get limit");

	if ((verbosestr = getenv("TRICKLE_VERBOSE")) == NULL)
		errx(1, "[trickle] Failed to get verbosity level");

	if ((argv0 = getenv("TRICKLE_ARGV")) == NULL)
		errx(1, "[trickle] Failed to get argv");

	if ((sockname = getenv("TRICKLE_SOCKNAME")) == NULL)
		errx(1, "[trickle] Failed to get socket name");

	if ((tsmoothstr = getenv("TRICKLE_TSMOOTH")) == NULL)
		errx(1, "[trickle] Failed to get time smoothing parameter");

	if ((lsmoothstr = getenv("TRICKLE_LSMOOTH")) == NULL)
		errx(1, "[trickle] Failed to get length smoothing parameter");

/*
	if ((latencystr = getenv("TRICKLE_LATENCY")) == NULL)
		errx(1, "[trickle] Failed to get length latency parameter");
*/

	winsz = atoi(winszstr) * 1024;
	lim[TRICKLE_RECV] = atoi(recvlimstr) * 1024;
	lim[TRICKLE_SEND] = atoi(sendlimstr) * 1024;
/* 	latency = atoi(latencystr);*/
	verbose = atoi(verbosestr);
//	verbose = -1;
	if ((tsmooth = strtod(tsmoothstr, (char **)NULL)) <= 0.0)
		errx(1, "[trickle] Invalid time smoothing parameter");
	lsmooth = atoi(lsmoothstr) * 1024;

	TAILQ_INIT(&sdhead);

	/*
	 * Open controlling socket
	 */

	trickled_configure(sockname, libc_socket, libc_read,
	    libc_write, libc_close, argv0);
	trickled_open(&trickled);

	bwstat_init(winsz);

	safe_printv(1, "[trickle] Initialized");

	initialized = 1;
}

int
socket(int domain, int type, int protocol)
{
	int sock;
	struct sockdesc *sd;

	INIT;

	sock = (*libc_socket)(domain, type, protocol);

#ifdef DEBUG
	safe_printv(0, "[DEBUG] socket(%d, %d, %d) = %d",
	    domain, type, protocol, sock);
#endif /* DEBUG */

	if (sock != -1 && domain == AF_INET && type == SOCK_STREAM) {
		if ((sd = calloc(1, sizeof(*sd))) == NULL)
			return (-1);
		if ((sd->stat = bwstat_new()) == NULL) {
			free(sd);
			return (-1);
		}

		/* All sockets are equals. */
		sd->stat->pts = 1;
		sd->stat->lsmooth = lsmooth;
		sd->stat->tsmooth = tsmooth;
		sd->sock = sock;

		TAILQ_INSERT_TAIL(&sdhead, sd, next);
	}

	return (sock);
}

int
close(int fd)
{
	struct sockdesc *sd, *next;

	INIT;

#ifdef DEBUG
	safe_printv(0, "[DEBUG] close(%d)", fd);
#endif /* DEBUG */

	for (sd = TAILQ_FIRST(&sdhead); sd != NULL; sd = next) {
		next = TAILQ_NEXT(sd, next);
		if (sd->sock == fd) {
			TAILQ_REMOVE(&sdhead, sd, next);
			bwstat_free(sd->stat);
			free(sd);
			break;
		}
	}

	if (fd == trickled) {
		trickled_close(&trickled);
		trickled_open(&trickled);
	}

	return ((*libc_close)(fd));
}

static struct delay *
select_delay(struct delayhead *dhead, struct sockdesc *sd, short which)
{
	ssize_t len = -1;
	struct timeval *delaytv;
	struct delay *d, *_d;

	updatesd(sd, 0, which);

	if ((delaytv = getdelay(sd, &len, which)) == NULL)
		return (NULL);

	safe_printv(3, "[trickle] Delaying socket (%s) %d "
	    "by %ld seconds %ld microseconds",
	    which == 0 ? "write" : "read", sd->sock,
	    delaytv->tv_sec, delaytv->tv_usec);

	if ((d = calloc(1, sizeof(*d))) == NULL)
		return (NULL);

	gettimeofday(&d->abstv, NULL);
	d->delaytv = *delaytv;
	d->which = which;
	d->sd = sd;
	sd->data[which].selectlen = len;

	if (TAILQ_EMPTY(dhead))
		TAILQ_INSERT_HEAD(dhead, d, next);
	else {
		TAILQ_FOREACH(_d, dhead, next)
			if (timercmp(&d->delaytv, &_d->delaytv, <)) {
				TAILQ_INSERT_BEFORE(_d, d, next);
				break;
			}
		if (_d == NULL)
			TAILQ_INSERT_TAIL(dhead, d, next);
	}

	return (d);
}

static struct delay *
select_shift(struct delayhead *dhead, struct timeval *inittv,
    struct timeval **delaytv)
{
	struct timeval curtv, difftv;
	struct delay *d;
	struct sockdesc *sd;

	gettimeofday(&curtv, NULL);
	timersub(&curtv, inittv, &difftv);

	TAILQ_FOREACH(d, dhead, next) {
		if (timercmp(&d->delaytv, &difftv, >))
			break;
		sd = d->sd;

		updatesd(sd, 0, d->which);
		SET(sd->data[d->which].flags, SD_INSELECT);
	}

	if (d != NULL)
		timersub(&d->delaytv, &difftv, *delaytv);
	else 
		*delaytv = NULL;

	/* XXX this should be impossible ... */
	if (*delaytv != NULL &&
	    ((*delaytv)->tv_sec < 0 || (*delaytv)->tv_usec < 0))
		timerclear(*delaytv);

	return (d);
}

int
_select(int nfds, fd_set *rfds, fd_set *wfds, fd_set *efds,
    struct timeval *__timeout)
{
	struct sockdesc *sd;
	fd_set *fdsets[] = { wfds, rfds }, *fds;
	struct timeval *delaytv, *selecttv = NULL, *timeout = NULL, _timeout,
	    inittv, curtv, difftv;
	short which;
	struct delayhead dhead;
	struct delay *d, *_d;
	int ret;

	INIT;

#ifdef DEBUG
	safe_printv(0, "[DEBUG] select(%d)", nfds);
#endif /* DEBUG */

	TAILQ_INIT(&dhead);

	/* We don't want to modify the user's timeout */
	if (__timeout != NULL) {
		_timeout = *__timeout;
		timeout = &_timeout;
	} 

	/*
	 * Sockets that require delaying get added to the delay list.
	 * delaytv is always assigned to the head of the list.
	 */
	for (which = 0; which < 2; which++)
		TAILQ_FOREACH(sd, &sdhead, next)
			if ((fds = fdsets[which]) != NULL && 
			    FD_ISSET(sd->sock, fds) &&
			    select_delay(&dhead, sd, which)) {
				FD_CLR(sd->sock, fds);
				nfds--;
			}

	gettimeofday(&inittv, NULL);
	curtv = inittv;
	d = TAILQ_FIRST(&dhead);
	delaytv = d != NULL ? &d->delaytv : NULL;
 again:
	timersub(&inittv, &curtv, &difftv);
	selecttv = NULL;

	if (delaytv != NULL)
		selecttv = delaytv;

	if (timeout != NULL) {
		timersub(timeout, &difftv, timeout);
		if (timeout->tv_sec < 0 || timeout->tv_usec < 0)
			timerclear(timeout);
		if (delaytv != NULL && timercmp(timeout, delaytv, <))
			selecttv = timeout;
		else if (delaytv == NULL)
			selecttv = timeout;
	}


#ifdef DEBUG
	safe_printv(0, "[DEBUG] IN select(%d)", nfds);
#endif /* DEBUG */

	ret = (*libc_select)(nfds, rfds, wfds, efds, selecttv);

#ifdef DEBUG
	safe_printv(0, "[DEBUG] OUT select(%d) = %d", nfds, ret);
#endif /* DEBUG */

	if (ret == 0 && delaytv != NULL && selecttv == delaytv) {
		_d = select_shift(&dhead, &inittv, &delaytv);
		while ((d = TAILQ_FIRST(&dhead)) != _d) {
			FD_SET(d->sd->sock, fdsets[d->which]);
			nfds++;
			TAILQ_REMOVE(&dhead, d, next);
			free(d);
		}

		gettimeofday(&curtv, NULL);
		goto again;
	}

	while ((d = TAILQ_FIRST(&dhead)) != NULL) {
		CLR(d->sd->data[d->which].flags, SD_INSELECT);
		TAILQ_REMOVE(&dhead, d, next);
		free(d);
	}

	return (ret);
}

#define POLL_WRMASK (POLLOUT | POLLWRNORM | POLLWRBAND)
#define POLL_RDMASK (POLLIN | /* POLLNORM | */  POLLPRI | POLLRDNORM | POLLRDBAND)

#if defined(__linux__) || (defined(__svr4__) && defined(__sun__)) || defined(__OpenBSD__)
int
poll(struct pollfd *fds, nfds_t nfds, int __timeout)
#elif defined(__FreeBSD__)
int
poll(struct pollfd *fds, unsigned int nfds, int __timeout)
#else
int
poll(struct pollfd *fds, int nfds, int __timeout)
#endif /* __linux__ */
{
	struct pollfd *pfd;
	int i, polltimeout, ret;
	struct sockdesc *sd;
	struct delay *d, *_d;
	struct timeval inittv, curtv, _timeout, *timeout = NULL, *delaytv,
	    *polltv, difftv;
	struct delayhead dhead;

	INIT;

#if defined(DEBUG) || defined(DEBUG_POLL)
	safe_printv(0, "[DEBUG] poll(*, %d, %d)", nfds, __timeout);
#endif /* DEBUG */

	if (__timeout != INFTIM) {
		_timeout.tv_sec = __timeout / 1000;
		_timeout.tv_usec = (__timeout % 1000) * 100;
		timeout = &_timeout;
	}

	TAILQ_INIT(&dhead);

	for (i = 0; i < nfds; i++) {
		pfd = &fds[i];
		TAILQ_FOREACH(sd, &sdhead, next)
			if (sd->sock == pfd->fd)
				break;
		if (sd == NULL)
			continue;

		/* For each event */
		if (pfd->events & POLL_RDMASK && 
		    (d = select_delay(&dhead, sd, TRICKLE_RECV)) != NULL) {
			d->pollevents = pfd->events & POLL_RDMASK;
			d->pollidx = i;
			pfd->events &= ~POLL_RDMASK;
		}

		if (pfd->events & POLL_WRMASK && 
		    (d = select_delay(&dhead, sd, TRICKLE_SEND)) != NULL) {
			d->pollevents = pfd->events & POLL_WRMASK;
			d->pollidx = i;
			pfd->events &= ~POLL_WRMASK;
		}
	}

	gettimeofday(&inittv, NULL);
	curtv = inittv;
	d = TAILQ_FIRST(&dhead);
	delaytv = d != NULL ? &d->delaytv : NULL;
 again:
	timersub(&inittv, &curtv, &difftv);
	polltv = NULL;

	if (delaytv != NULL)
		polltv = delaytv;

	if (timeout != NULL) {
		timersub(timeout, &difftv, timeout);
		if (delaytv != NULL && timercmp(timeout, delaytv, <))
			polltv = timeout;
		else if (delaytv == NULL)
			polltv = timeout;
	}

	/* Calculate polltimeout here based on polltv */
	if (polltv != NULL)
		polltimeout = polltv->tv_sec * 1000 +
		    polltv->tv_usec / 100;
	else
		polltimeout = INFTIM;

#if defined(DEBUG) || defined(DEBUG_POLL)
	safe_printv(0, "[DEBUG] IN poll(*, %d, %d)", nfds, polltimeout);
#endif /* DEBUG */

	ret = (*libc_poll)((struct pollfd *)fds, (int)nfds, (int)polltimeout);

#if defined(DEBUG) || defined(DEBUG_POLL)
	safe_printv(0, "[DEBUG] OUT poll(%d) = %d", nfds, ret);
#endif /* DEBUG */

	if (ret == 0 && delaytv != NULL && polltv == delaytv) {
		_d = select_shift(&dhead, &inittv, &delaytv);
		while ((d = TAILQ_FIRST(&dhead)) != NULL && d != _d) {
			fds[d->pollidx].events |= d->pollevents;

			TAILQ_REMOVE(&dhead, d, next);
			free(d);
		}

		gettimeofday(&curtv, NULL);
		goto again;
	}

	while ((d = TAILQ_FIRST(&dhead)) != NULL) {
		CLR(d->sd->data[d->which].flags, SD_INSELECT);

		TAILQ_REMOVE(&dhead, d, next);
		free(d);
	}

	return (ret);
}

ssize_t
read(int fd, void *buf, size_t nbytes)
{
	ssize_t ret = -1;
	size_t xnbytes = nbytes;
	int eagain;

	INIT;

	if (!(eagain = delay(fd, &xnbytes, TRICKLE_RECV) == TRICKLE_WOULDBLOCK)) {
		ret = (*libc_read)(fd, buf, xnbytes);
#ifdef DEBUG
		safe_printv(0, "[DEBUG] read(%d, *, %d) = %d", fd, xnbytes, ret);
	} else {
		safe_printv(0, "[DEBUG] delaying read(%d, *, %d) = %d", fd, xnbytes, ret);
#endif /* DEBUG */
	}

	update(fd, ret, TRICKLE_RECV);

	if (eagain) {
		ret = -1;
		errno = EAGAIN;
	}

	return (ret);
}

/*
 * XXX defunct for smoothing ... for now
 */
ssize_t
readv(int fd, const struct iovec *iov, int iovcnt)
{
	size_t len = 0;
	ssize_t ret = -1;
	int i, eagain;

	INIT;


	for (i = 0; i < iovcnt; i++)
		len += iov[i].iov_len;

	if (!(eagain = delay(fd, &len, TRICKLE_RECV) == TRICKLE_WOULDBLOCK)) {
		ret = (*libc_readv)(fd, iov, iovcnt);
#ifdef DEBUG
		safe_printv(0, "[DEBUG] readv(%d, *, %d) = %d", fd, iovcnt, ret);
	} else {
		safe_printv(0, "[DEBUG] delaying readv(%d, *, %d)", fd, iovcnt);
#endif /* DEBUG */
	}

	update(fd, ret, TRICKLE_RECV);

	if (eagain) {
		errno = EAGAIN;
		ret = -1;
	}

	return (ret);
}

#ifndef __FreeBSD__ 
ssize_t
recv(int sock, void *buf, size_t len, int flags)
{
	ssize_t ret = -1;
	size_t xlen = len;
	int eagain;

	INIT;

	if (!(eagain = delay(sock, &xlen, TRICKLE_RECV) == TRICKLE_WOULDBLOCK)) {
		ret = (*libc_recv)(sock, buf, xlen, flags);
#ifdef DEBUG
		safe_printv(0, "[DEBUG] recv(%d, *, %d, %d) = %d",
		    sock, len, flags, ret);
	} else {
		safe_printv(0, "[DEBUG] delaying recv(%d, *, %d, %d)", sock, len, flags);		
#endif /* DEBUG */
	}

	update(sock, ret, TRICKLE_RECV);

	if (eagain) {
		errno = EAGAIN;
		ret = -1;
	}

	return (ret);
}
#endif /* !__FreeBSD__ */

#ifdef __sun__
ssize_t
recvfrom(int sock, void *buf, size_t len, int flags, struct sockaddr *from,
    Psocklen_t fromlen)
#else
ssize_t
recvfrom(int sock, void *buf, size_t len, int flags, struct sockaddr *from,
    socklen_t *fromlen)
#endif /* __sun__ */
{
	ssize_t ret = -1;
	size_t xlen = len;
	int eagain;

	INIT;

	if (!(eagain = delay(sock, &xlen, TRICKLE_RECV) == TRICKLE_WOULDBLOCK)) {
		ret = (*libc_recvfrom)(sock, buf, xlen, flags, from, fromlen);
#ifdef DEBUG
		safe_printv(0, "[DEBUG] recvfrom(%d, *, %d, %d) = %d",
		    sock, len, flags, ret);
	} else {
		safe_printv(0, "[DEBUG] delaying recvfrom(%d, *, %d, %d)", sock,
		    len, flags);		
#endif /* DEBUG */
	}

	update(sock, ret, TRICKLE_RECV);

	if (eagain) {
		errno = EAGAIN;
		ret = -1;
	}

	return (ret);
}

ssize_t
write(int fd, const void *buf, size_t len)
{
	ssize_t ret = -1;
	size_t xlen = len;
	int eagain;

	INIT;

	if (!(eagain = delay(fd, &xlen, TRICKLE_SEND) == TRICKLE_WOULDBLOCK)) {
		ret = (*libc_write)(fd, buf, xlen);
#ifdef DEBUG
		safe_printv(0, "[DEBUG] write(%d, *, %d) = %d", fd, len, ret);
	} else {
		safe_printv(0, "[DEBUG] delaying write(%d, *, %d)", fd, len);
#endif /* DEBUG */
	}

	update(fd, ret, TRICKLE_SEND);

	if (eagain) {
		errno = EAGAIN;
		ret = -1;
	}

	return (ret);
}

/*
 * XXX defunct for smoothing ... for now
 */
ssize_t
writev(int fd, const struct iovec *iov, int iovcnt)
{
	ssize_t ret = -1;
	size_t len = 0;
	int i, eagain;

	INIT;

	for (i = 0; i < iovcnt; i++)
		len += iov[i].iov_len;

	if (!(eagain = delay(fd, &len, TRICKLE_SEND) == TRICKLE_WOULDBLOCK)) {
		ret = (*libc_writev)(fd, iov, iovcnt);
#ifdef DEBUG
		safe_printv(0, "[DEBUG] writev(%d, *, %d) = %d",
		    fd, iovcnt, ret);
	} else {
		safe_printv(0, "[DEBUG] delaying writev(%d, *, %d)", fd, iovcnt);
#endif /* DEBUG */
	}

	update(fd, ret, TRICKLE_SEND);

	if (eagain) {
		errno = EAGAIN;
		ret = -1;
	}

	return (ret);
}

#ifndef __FreeBSD__
ssize_t
send(int sock, const void *buf, size_t len, int flags)
{
	ssize_t ret = -1;
	size_t xlen = len;
	int eagain;

	INIT;

	if (!(eagain = delay(sock, &xlen, TRICKLE_SEND) == TRICKLE_WOULDBLOCK)) {
		ret = (*libc_send)(sock, buf, xlen, flags);
#ifdef DEBUG
		safe_printv(0, "[DEBUG] send(%d, *, %d, %d) = %d",
		    sock, len, flags, ret);
	} else {
		safe_printv(0, "[DEBUG] delaying send(%d, *, %d, %d)", sock,
		    len, flags);
#endif /* DEBUG */
	}

	update(sock, ret, TRICKLE_SEND);

	if (eagain) {
		errno = EAGAIN;
		ret = -1;
	}

	return (ret);
}
#endif /* !__FreeBSD__ */

ssize_t
sendto(int sock, const void *buf, size_t len, int flags, const struct sockaddr *to,
    socklen_t tolen)
{
	ssize_t ret = -1;
	size_t xlen = len;
	int eagain;

	INIT;

	if (!(eagain = delay(sock, &xlen, TRICKLE_SEND) == TRICKLE_WOULDBLOCK)) {
		ret = (*libc_sendto)(sock, buf, xlen, flags, to, tolen);
#ifdef DEBUG
		safe_printv(0, "[DEBUG] sendto(%d, *, %d) = %d", sock, len, ret);
	} else {
		safe_printv(0, "[DEBUG] delaying sendto(%d, *, %d)", sock, len);
#endif /* DEBUG */
	}

	update(sock, ret, TRICKLE_SEND);

	if (eagain) {
		errno = EAGAIN;
		ret = -1;
	}

	return (ret);
}

#if 0
int
setsockopt(int sock, int level, int optname, const void *optval,
    socklen_t option)
{
	INIT;

	/* blocking, etc. */
	return ((*libc_setsockopt)(sock, level, optname, optval, option));
}
#endif /* 0 */

int
dup(int oldfd)
{
	int newfd;
	struct sockdesc *sd, *nsd;

	INIT;

	newfd = (*libc_dup)(oldfd);

#ifdef DEBUG
	safe_printv(0, "[DEBUG] dup(%d) = %d", oldfd, newfd);
#endif /* DEBUG */

	TAILQ_FOREACH(sd, &sdhead, next)
	        if (oldfd == sd->sock)
			break;

	if (sd != NULL && newfd != -1) {
		if ((nsd = malloc(sizeof(*nsd))) == NULL) {
			(*libc_close)(newfd);
			return (-1);
		}
		sd->sock = newfd;
		memcpy(nsd, sd, sizeof(*nsd));
		TAILQ_INSERT_TAIL(&sdhead, nsd, next);
	}

	return (newfd);
}

int
dup2(int oldfd, int newfd)
{
	struct sockdesc *sd, *nsd;
	int ret;

	INIT;

	ret = (*libc_dup2)(oldfd, newfd);

#ifdef DEBUG
	safe_printv(0, "[DEBUG] dup2(%d, %d) = %d", oldfd, newfd, ret);
#endif /* DEBUG */

	TAILQ_FOREACH(sd, &sdhead, next)
		if (oldfd == sd->sock)
			break;

	if (sd != NULL && ret != -1) {
		if ((nsd = malloc(sizeof(*nsd))) == NULL)
			return (-1);
		sd->sock = newfd;
		memcpy(nsd, sd, sizeof(*nsd));
		TAILQ_INSERT_TAIL(&sdhead, nsd, next);
	}

	return (ret);
}

#ifdef __sun__
int
accept(int sock, struct sockaddr *addr, Psocklen_t addrlen)
#else
int
accept(int sock, struct sockaddr *addr, socklen_t *addrlen)
#endif /* __sun__ */
{
	int ret;
	struct sockdesc *sd;

	INIT;

	ret = (*libc_accept)(sock, addr, addrlen);

#ifdef DEBUG
	safe_printv(0, "[DEBUG] accept(%d) = %d", sock, ret);
#endif /* DEBUG */

	if (ret != -1) {
		if ((sd = calloc(1, sizeof(*sd))) == NULL)
			return (ret);

		if ((sd->stat = bwstat_new()) == NULL) {
			free(sd);
			return (ret);
		}

		sd->sock = ret;
		sd->stat->pts = 1;
		sd->stat->lsmooth = lsmooth;
		sd->stat->tsmooth = tsmooth;
		TAILQ_INSERT_TAIL(&sdhead, sd, next);
	}

	return (ret);
}

#ifdef HAVE_SENDFILE
ssize_t
sendfile(int out_fd, int in_fd, off_t *offset, size_t count)
{
	size_t inbytes = count, outbytes = count, bytes;
	ssize_t ret = 0;

	INIT;

	/* in_fd = recv, out_fd = send */

	/* We should never get TRICKLE_WOULDBLOCK here */
	delay(in_fd, &inbytes, TRICKLE_RECV);
	delay(out_fd, &outbytes, TRICKLE_SEND);

	/* This is a slightly ugly hack. */
	bytes = MIN(inbytes, outbytes);
	if (bytes > 0)
		ret = (*libc_sendfile)(out_fd, in_fd, offset, bytes);

	return (ret);
}
#endif	/* HAVE_SENDFILE */

static int
delay(int sock, ssize_t *len, short which)
{
	struct sockdesc *sd;
	struct timeval *tv;
	struct timespec ts, rm;

	TAILQ_FOREACH(sd, &sdhead, next)
		if (sock == sd->sock)
			break;

	if (sd == NULL)
		return (-1);

	if (ISSET(sd->data[which].flags, SD_INSELECT)) {
		if (*len > sd->data[which].selectlen)
			*len = sd->data[which].selectlen;
		CLR(sd->data[which].flags, SD_INSELECT);
		return (0);
	}

	if ((tv = getdelay(sd, len, which)) != NULL) {
		TIMEVAL_TO_TIMESPEC(tv, &ts);

		safe_printv(2, "[trickle] Delaying %lds%ldus",
		    tv->tv_sec, tv->tv_usec);

		if (ISSET(sd->flags, TRICKLE_NONBLOCK))
			return (TRICKLE_WOULDBLOCK);

		while (nanosleep(&ts, &rm) == -1 && errno == EINTR)
			ts = rm;
	}

	return (0);
}

static struct timeval *
getdelay(struct sockdesc *sd, ssize_t *len, short which)
{
	struct timeval *xtv;
	uint xlim = lim[which];

	/* XXX check this. */
	if (*len < 0)
		*len = sd->data[which].lastlen;

	if (trickled && (xtv = trickled_getdelay(which, len)) != NULL)
		xlim = *len / (xtv->tv_sec + xtv->tv_usec / 1000000.0);

	if (xlim == 0)
		return (NULL);

	return (bwstat_getdelay(sd->stat, len, xlim, which));
}

static void
update(int sock, ssize_t len, short which)
{
	struct sockdesc *sd;

	TAILQ_FOREACH(sd, &sdhead, next)
		if (sock == sd->sock)
			break;

	if (sd == NULL)
		return;

	updatesd(sd, len, which);
}

static void
updatesd(struct sockdesc *sd, ssize_t len, short which)
{
	struct bwstat_data *bsd;
	int ret;

	if (len < 0)
		len = 0;

	if ((ret = fcntl(sd->sock, F_GETFL, 0)) != -1) {
		if (ret & O_NONBLOCK)
			SET(sd->flags, TRICKLE_NONBLOCK);
		else
			CLR(sd->flags, TRICKLE_NONBLOCK);
	}

	if (len > 0)
		sd->data[which].lastlen = len;

	if (trickled)
		trickled_update(which, len);

	bwstat_update(sd->stat, len, which);

	bsd = &sd->stat->data[which];

	safe_printv(1, "[trickle] avg: %d.%d KB/s; win: %d.%d KB/s",
	    (bsd->rate / 1024), ((bsd->rate % 1024) * 100 / 1024),
	    (bsd->winrate / 1024), ((bsd->winrate % 1024) * 100 / 1024));
}

void
safe_printv(int level, const char *fmt, ...)
{
	va_list ap;
	char str[1024];
	int n;

	if (level > verbose)
		return;

	va_start(ap, fmt);

	if ((n = snprintf(str, sizeof(str), "%s: ", argv0)) == -1) {
		str[0] = '\0';
		n = 0;
	}

        if (fmt != NULL)
		n = vsnprintf(str + n, sizeof(str) - n, fmt, ap);

	if (n == -1)
		return;

	strlcat(str, "\n", sizeof(str));

	(*libc_write)(STDERR_FILENO, str, strlen(str));
	va_end(ap);
}
