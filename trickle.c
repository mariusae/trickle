/*
 * trickle.c
 *
 * Copyright (c) 2002, 2003 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id: trickle.c,v 1.18 2004/02/13 06:13:05 marius Exp $
 */

#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/param.h>
#include <sys/stat.h>

#ifdef HAVE_ERR_H
#include <err.h>
#endif /* HAVE_ERR_H */
#include <errno.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "util.h"

size_t strlcat(char *, const char *, size_t);
void   usage(void);

#ifdef HAVE___PROGNAME
extern char *__progname;
#else
char *__progname;
#endif

#define LIBNAME "trickle-overload.so"

int
main(int argc, char **argv)
{
	char *winsz = "200", verbosestr[16],
	    *uplim = "10", *downlim = "10", *tsmooth = "3.0", *lsmooth = "20",
	    *latency = "0";
	int opt, verbose = 0, standalone = 0;
	char buf[MAXPATHLEN], sockname[MAXPATHLEN], *path, **pathp;
	struct stat sb;
	char *trypaths[]  = {
		LIBNAME,
		LIBDIR "/" LIBNAME,
		NULL
	};

	__progname = get_progname(argv[0]);
	sockname[0] = '\0';

	while ((opt = getopt(argc, argv, "hvVsw:n:u:d:t:l:L:")) != -1)
                switch (opt) {
		case 'v':
			verbose++;
			break;
		case 'w':
			winsz = optarg;
			break;
		case 'u':
			uplim = optarg;
			break;
		case 'd':
			downlim = optarg;
			break;
		case 'V':
			errx(1, "version " VERSION);
			break;
		case 'n':
			strlcpy(sockname, optarg, sizeof(sockname));
			break;
		case 't':
			tsmooth = optarg;
			break;
		case 'l':
			lsmooth = optarg;
			break;
		case 's':
			standalone = 1;
			break;
		case 'L':
			latency = optarg;
			break;
                case 'h':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	for (pathp = trypaths; *pathp != NULL; pathp++)
		if (lstat(*pathp, &sb) == 0)
			break;

	path = *pathp;
	if (path == NULL)
		errx(1, "Could not find overload object");

	if (path[0] != '/') {
		if (getcwd(buf, sizeof(buf)) == NULL)
			errx(1, "getcwd");

		strlcat(buf, "/", sizeof(buf));
		strlcat(buf, path, sizeof(buf));

		path = buf;
	}

	if (!standalone) {
		if (sockname[0] == '\0')
			strlcpy(sockname, "/tmp/.trickled.sock",
			    sizeof(sockname));

		if (stat(sockname, &sb) == -1 &&
		    (errno == EACCES || errno == ENOENT))
			warn("Could not reach trickled, working independently");
	} else
		strlcpy(sockname, "", sizeof(sockname));

	snprintf(verbosestr, sizeof(verbosestr), "%d", verbose);

	setenv("TRICKLE_DOWNLOAD_LIMIT", downlim, 1);
	setenv("TRICKLE_UPLOAD_LIMIT", uplim, 1);
	setenv("TRICKLE_VERBOSE", verbosestr, 1);
	setenv("TRICKLE_WINDOW_SIZE", winsz, 1);
	setenv("TRICKLE_ARGV", argv[0], 1);
	setenv("TRICKLE_SOCKNAME", sockname, 1);
	setenv("TRICKLE_TSMOOTH", tsmooth, 1);
	setenv("TRICKLE_LSMOOTH", lsmooth, 1);
/*	setenv("TRICKLE_LATENCY", latency, 1); */

	setenv("LD_PRELOAD", path, 1);

	execvp(argv[0], argv);
	err(1, "exec()");

	/* NOTREACHED */
	return (1);
}

void
usage(void)
{
	fprintf(stderr,
	    "Usage: %s [-hvVs] [-d <rate>] [-u <rate>] [-w <length>] "
	    "[-t <seconds>]\n"
	    "       %*c [-l <length>] [-n <path>] command ...\n"
	    "\t-h           Help (this)\n"
	    "\t-v           Increase verbosity level\n"
	    "\t-V           Print %s version\n"
	    "\t-s           Run trickle in standalone mode independent of trickled\n"
	    "\t-d <rate>    Set maximum cumulative download rate to <rate> KB/s\n"
	    "\t-u <rate>    Set maximum cumulative upload rate to <rate> KB/s\n"
	    "\t-w <length>  Set window length to <length> KB \n"
	    "\t-t <seconds> Set default smoothing time to <seconds> s\n"
	    "\t-l <length>  Set default smoothing length to <length> KB\n"
	    "\t-n <path>    Use trickled socket name <path>\n"
	    "\t-L <ms>      Set latency to <ms> milliseconds\n",
	    __progname, (int)strlen(__progname), ' ', __progname);

	exit(1);
}
