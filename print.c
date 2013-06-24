/*
 * print.c
 *
 * Copyright (c) 2001, 2002 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id: print.c,v 1.5 2003/03/06 05:54:44 marius Exp $
 */

/*
 * Some code adopted from:
 *
 * err.c
 *
 * Adapted from OpenBSD libc *err* *warn* code.
 *
 * Copyright (c) 2000 Dug Song <dugsong@monkey.org>
 *
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 */

#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

static void vprint(const char *, va_list);
static void vprintx(const char *, va_list);

static int verbose, use_syslog;

extern char *__progname;

void
print_setup(int _verbose, int _use_syslog)
{
	verbose = _verbose;
	use_syslog = _use_syslog;

	if (use_syslog)
		openlog(__progname, LOG_PID, LOG_DAEMON);
}

/*
 * These are adopted from the OpenBSD err*() and warn*() functions.
 */

void
errv(int level, int eval, const char *fmt, ...)
{
	va_list ap;
	
	if (level > verbose)
		exit(eval);

	va_start(ap, fmt);
	vprint(fmt, ap);
	va_end(ap);
	exit(eval);
}

void
errxv(int level, int eval, const char *fmt, ...)
{
	va_list ap;
	
	if (level > verbose)
		exit(eval);

	va_start(ap, fmt);
	vprintx(fmt, ap);
	va_end(ap);
	exit(eval);
}

void
warnv(int level, const char *fmt, ...)
{
	va_list ap;
	
	if (level > verbose)
		return;

	va_start(ap, fmt);
	vprint(fmt, ap);
	va_end(ap);
}

void
warnxv(int level, const char *fmt, ...)
{
	va_list ap;

	if (level > verbose)
		return;

	va_start(ap, fmt);
	vprintx(fmt, ap);
	va_end(ap);
}

static void
vprint(const char *fmt, va_list ap)
{
	if (use_syslog) {
		char msg[1024];
		if (fmt != NULL) {
			msg[0] = '\0';
			vsnprintf(msg, sizeof(msg), fmt, ap);
			strlcat(msg, ": ", sizeof(msg));
			strlcat(msg, strerror(errno), sizeof(msg));
			syslog(LOG_INFO, "%s", msg);
		}
		return;
	}

	fprintf(stderr, "%s: ", __progname);
        if (fmt != NULL)
                vfprintf(stderr, fmt, ap);
        fprintf(stderr, ": %s\n", strerror(errno));
}

static void
vprintx(const char *fmt, va_list ap)
{
	if (use_syslog) {
		if (fmt != NULL)
			vsyslog(LOG_INFO, fmt, ap);
		return;
	}

	fprintf(stderr, "%s: ", __progname);
        if (fmt != NULL)
                vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
}

/*
 * Assumes 80 character wide screen.  Use termcap, or etc.  to
 * determine real width and do this dynamically.
 *       if (ioctl(fileno(stdout), TIOCGWINSZ, &winsz) != -1)
 *               width = winsz.ws_col && winsz.ws_col < 256 ? winsz.ws_col : 80;
 *       else
 *               width = 80;
 */

/*
 * XXX only do if isatty(); ...
 */
void
print_dump(u_char *buf, int len)
{
	int i, j, goback;

	printf("%s: ", __progname);
	
	for (i = 0; i < len; ++i) {
		printf("%02x ", buf[i]);
		if ((goback = i % 16) == 15 || i == len - 1) {
			for (j = 15 - goback; j >= 0; j--) printf("   ");
			for (j = i - goback; j <= i; j++)
				if (buf[j] > 31 && buf[j] < 127)
					printf("%c", buf[j]);
				else
					printf(".");
			if (i != len - 1)
				printf("\n%s: ", __progname);
		} 
	}
	printf("\n");
}
