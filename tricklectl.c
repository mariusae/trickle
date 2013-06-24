/*
 * tricklectl.c
 *
 * Copyright (c) 2003 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id: tricklectl.c,v 1.4 2003/06/02 23:13:28 marius Exp $
 */

#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif /* HAVE_SYS_TIME_H */
#include <sys/socket.h>

#include <stdio.h>
#ifdef HAVE_ERR_H
#include <err.h>
#endif /* HAVE_ERR_H */
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#if defined(HAVE_TIME_H) && defined(TIME_WITH_SYS_TIME)
#include <time.h>
#endif /* defined(HAVE_TIME_H) && defined(TIME_WITH_SYS_TIME) */

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#include "message.h"
#include "trickledu.h"

static int trickled_sock;

void usage(void);
void handle_command(int, int, char **);

#define TRICKLED_SOCKNAME "/tmp/.trickled.sock"

#define COMMAND_GETRATES 0
#define COMMAND_LAST 1

static char *commands[] = {
	[COMMAND_GETRATES] = "getrates",
	[COMMAND_LAST]     = NULL
};

int
main(int argc, char **argv)
{
	char *sockname = TRICKLED_SOCKNAME;
	int opt, i;

	while ((opt = getopt(argc, argv, "hs:")) != -1)
                switch (opt) {
		case 's':
			sockname = optarg;
			break;
		case 'h':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	for (i = 0; commands[i] != NULL; i++)
		if (strlen(commands[i]) == strlen(argv[0]) &&
		    strcmp(commands[i], argv[0]) == 0)
			break;

	if (i == COMMAND_LAST)
		usage();

	argc -= 1;
	argv += 1;

	trickled_configure(sockname, &socket, &read, &write, &close, argv[0]);
	trickled_ctl_open(&trickled_sock);

	if (!trickled_sock)
		err(1, sockname);

	handle_command(i, argc, argv);

	return (0);
}

void
handle_command(int cmd, int ac, char **av)
{
	switch (cmd) {
	case COMMAND_GETRATES: {
		uint32_t uplim, uprate, downlim, downrate;
		if (trickled_getinfo(&uplim, &uprate, &downlim, &downrate) == -1)
			err(1, "trickled_getinfo()");
		/* XXX testing downlim, too, etc */
		warnx("DOWNLOAD: %d.%d KB/s (utilization: %.1f%%)",
		    downrate / 1024, (downrate % 1024) * 100 / 1024,
		    ((1.0 * downrate) / (1.0 * downlim)) * 100);
		if (uprate == 0)
			uprate = 1;
		warnx("UPLOAD: %d.%d KB/s (utilization: %.1f%%)",
		    uprate / 1024, (uprate % 1024) * 100 / 1024,
		    ((1.0 * uprate) / (1.0 * uplim)) * 100);
	}
	default:
		break;
	}
}

void
usage(void)
{
	exit(1);
}
