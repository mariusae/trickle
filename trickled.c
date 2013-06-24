/*
 * trickled.c
 *
 * Copyright (c) 2003 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id: trickled.c,v 1.24 2003/06/25 19:12:53 marius Exp $
 */

/*
 * NOTES
 * - on the first transmission; limit the #bytes to smoothing length to avoid 
 *   initial spike
 */

#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/tree.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/param.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif /* HAVE_SYS_TIME_H */

#include <netinet/in.h>
#include <netinet/in_systm.h>

#ifdef HAVE_ERR_H
#include <err.h>
#endif /* HAVE_ERR_H */
#include <stdio.h>
#include <stdlib.h>
#include <event.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <signal.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#if defined(HAVE_TIME_H) && defined(TIME_WITH_SYS_TIME)
#include <time.h>
#endif /* defined(HAVE_TIME_H) && defined(TIME_WITH_SYS_TIME) */

#include "trickle.h"
#include "print.h"
#include "util.h"
#include "message.h"
#include "client.h"
#include "conf.h"
#include "cleanup.h"

void        trickled_setup(char *);
void        newclientcb(int, short, void *);
static void killclient(struct client *);
void        msgcb(int, short, void *);
void        notifycb(int, short, void *);
void        usage(void);
void        gensigcb(int, short, void *);
void        forcecb(int, short, void *);

#define CONF_SAVE(w, f) do {            	\
	char *p = f;				\
	if (p != NULL)				\
		(w) = p;  			\
} while (0)

#ifdef HAVE___PROGNAME
extern char *__progname;
#else
char *__progname;
#endif

struct event listenev;
int winsz = 1024 * 200;
double tsmooth = 5;
uint lsmooth = 10, pri = 1, globlim[2] = { 10 * 1024, 10 * 1024 };
char *conf_path = SYSCONFDIR "/trickled.conf";
struct event sighupev, sigtermev, sigintev, notifyev, forceev;
cleanup_t *cleanup;
struct timeval notifytv, forcetv = {5, 0};

int
main(int argc, char **argv)
{
	int opt, verbose = 0, fg = 0, usesyslog = 0;
	struct stat sb;
	static char sockname[MAXPATHLEN];

	__progname = get_progname(argv[0]);

	print_setup(verbose, usesyslog);

	sockname[0] = '\0';

#define GETOPTSTR "Vvfhsu:d:c:l:t:p:w:n:N:"

        while ((opt = getopt(argc, argv, GETOPTSTR)) != -1)
                if (opt == 'c')
                        conf_path = optarg;
        optind = 1;

	if (stat(conf_path, &sb) == -1 && errno == ENOENT)
                warnv(0, "Skipping configuration file: %s", conf_path);

	while ((opt = getopt(argc, argv, GETOPTSTR)) != -1)
                switch (opt) {
		case 'v':
			verbose++;
			break;
		case 'u':
			globlim[TRICKLE_SEND] = 1024 * atoi(optarg);
			break;
		case 'd':
			globlim[TRICKLE_RECV] = 1024 * atoi(optarg);
			break;
		case 'f':
			fg = 1;
			break;
                case 'V':
                        warnxv(0, PACKAGE " version " VERSION);
                        exit(0);
		case 't':
			tsmooth = strtod(optarg, (char **)NULL);
			break;
		case 'l':
			lsmooth = atoi(optarg);
			break;
		case 's':
			usesyslog = 1;
			break;
		case 'p':
			pri = atoi(optarg);
			if (pri > 20 || pri < 0) {
				warnxv(0, "Invalid priority %d", pri);
				usage();
			}
			break;
		case 'w':
			winsz = 1024 * atoi(optarg);
			break;
		case 'n':
			strlcpy(sockname, optarg, sizeof(sockname));
			break;
		case 'N':
			notifytv.tv_sec = atoi(optarg);
			break;
		case 'c':
			break;
		case 'h':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	conf_init();
	print_setup(verbose, usesyslog);

	if (!fg && daemon(1, 0) == -1)
		errv(0, 1, "daemon()");

	event_init();

	if (sockname[0] == '\0')
		strlcpy(sockname, "/tmp/.trickled.sock", sizeof(sockname));

	if (stat(sockname, &sb) != -1)
		errxv(0, 1, "Socket name %s not free", sockname);

	if ((cleanup = cleanup_new()) == NULL)
		errxv(0, 1, "Failed setting up cleanup functionality");
	client_init(winsz);
	trickled_setup(sockname);
	warnxv(1, "Using socket name: %s", sockname);

	signal_set(&sigtermev, SIGTERM, gensigcb, NULL);
	if (signal_add(&sigtermev, NULL) == -1)
		errv(0, 1, "signal_add()");
	signal_set(&sigintev, SIGINT, gensigcb, NULL);
	if (signal_add(&sigintev, NULL) == -1)
		errv(0, 1, "signal_add()");

	if (timerisset(&notifytv)) {
		evtimer_set(&notifyev, notifycb, NULL);
		evtimer_add(&notifyev, &notifytv);
	}

	evtimer_set(&forceev, forcecb, NULL);
	evtimer_add(&forceev, &forcetv);

	event_dispatch();

	err(1, "event error");

	/* NOTREACHED */
	return (1);
}

void
trickled_setup(char *sockname)
{
	int s;
	struct sockaddr_un xsun;
	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket()");

	memset(&xsun, 0, sizeof(xsun));
	xsun.sun_family = AF_UNIX;
	strlcpy(xsun.sun_path, sockname, sizeof(xsun.sun_path));

	if (bind(s, (struct sockaddr *)&xsun, sizeof(xsun)) == -1)
		errv(0, 1, "bind()");

	/* We want to serve everybody */
	if (chmod(sockname, S_IRWXU | S_IRWXG | S_IRWXO) == -1)
		errv(0, 1, "chmod()");

	if (listen(s, 10) == -1)
		errv(0, 1, "listen()");

	event_set(&listenev, s, EV_READ, newclientcb, NULL);
	if (event_add(&listenev, NULL) == -1)
		errv(0, 1, "event_add()");

	/* Make sure we are good citizens */
	cleanup_add(cleanup, cleanupcb_unlink, sockname);
}

void
forcecb(int fd, short which, void *arg)
{
	client_force();

	if (evtimer_add(&forceev, &forcetv) == -1)
		err(1, "event_add()");
}

void
newclientcb(int fd, short which, void *arg)
{
	struct sockaddr sa;
	int s;
	socklen_t salen = sizeof(sa);
	struct client *cli;

	if ((s = accept(fd, &sa, &salen)) == -1) {
		warn("accept()");
		goto out;
	}

	if ((cli = calloc(1, sizeof(*cli))) == NULL) {
		warn("calloc()");
		goto out;
	}

	cli->s = s;
	event_set(&cli->ev, s, EV_READ, msgcb, cli);

	if (event_add(&cli->ev, NULL) == -1)
		warn("event_add()");

 out:
	if (event_add(&listenev, NULL) == -1)
		err(1, "event_add()");
}

void
msgcb(int fd, short which, void *arg)
{
	struct client *cli = arg;
	struct msg msg;

	if (client_recvmsg(cli, &msg) == -1) {
		killclient(cli);
		return;
	}

	switch (msg.type) {
	case MSG_TYPE_SPECTATOR:
		SET(cli->flags, CLIENT_CONFIGURED | CLIENT_SPECTATOR);

		goto out;
		break;
	case MSG_TYPE_CONF: {
		struct passwd *pw;
		struct group *gr;
		struct msg_conf *conf = &msg.data.conf;

		cli->pid = conf->pid;
		cli->uid = conf->uid;
		cli->gid = conf->gid;
		memcpy(cli->lim, conf->lim, sizeof(cli->lim));
		strlcpy(cli->argv0, conf->argv0, sizeof(cli->argv0));

		/* XXX These kills seems kind of brutal */
		if ((pw = getpwuid(cli->uid)) != NULL) {
			strlcpy(cli->uname, pw->pw_name, sizeof(cli->uname));
		} else {
			warnxv(1, "Failed to translate UID to name");
			killclient(cli);
			return;
		}
		if ((gr = getgrgid(cli->gid)) != NULL) {
			strlcpy(cli->gname, gr->gr_name, sizeof(cli->gname));
		} else {
			warnxv(1, "Failed to translate GID to name");
			killclient(cli);
			return;
		}

                cli->pri = conf_get_num(cli->argv0, "Priority", pri);
		cli->tsmooth = conf_get_fnum(cli->argv0, "Time-Smoothing",
		    tsmooth);
		cli->lsmooth = conf_get_num(cli->argv0, "Length-Smoothing", 
		    lsmooth);

		if (client_register(cli) == -1) {
			warnxv(1, "Failed to register client");
			killclient(cli);
			return;
		}

		if (client_configure(cli) == -1) {
			warnxv(1, "Client configuration error");
			killclient(cli);
			return;
		}

		SET(cli->flags, CLIENT_CONFIGURED);

		warnxv(1, "New client: %d (%s/%s)", cli->pid, cli->argv0,
		    cli->uname);

		goto out;
		break;
	}
	default:
		if (!ISSET(cli->flags, CLIENT_CONFIGURED))
			goto out;
	}

	/* Spectators have a limited "command set." */
	if (msg.type < MSG_TYPE_GETINFO &&
	    ISSET(cli->flags, CLIENT_SPECTATOR))
		goto out;

	switch (msg.type) {
	case MSG_TYPE_UPDATE: {
		struct msg_update *update = &msg.data.update;
		client_update(cli, update->dir, update->len);
		break;
	}
	case MSG_TYPE_DELAY: {
		struct msg_delay *delay = &msg.data.delay;
 		client_delay(cli, delay->dir, delay->len, globlim[delay->dir]);
		break;
	}
	case MSG_TYPE_GETDELAY: {
		struct msg_delay *delay = &msg.data.delay;
		client_getdelay(cli, delay->dir, delay->len,
		    globlim[delay->dir]);
		break;
	}
	case MSG_TYPE_GETINFO: {
		client_getinfo(cli, globlim[TRICKLE_SEND],
		    globlim[TRICKLE_RECV]);
		break;
	}
	default:
		warnxv(0, "Unknown message type %d", msg.type);
		break;
	}

 out:
	if (event_add(&cli->ev, NULL) == -1)
		warn("event_add()");
}

static void
killclient(struct client *cli)
{
	close(cli->s);

	if (ISSET(cli->flags, CLIENT_CONFIGURED)) {
		if (!ISSET(cli->flags, CLIENT_SPECTATOR)) {
			client_unregister(cli);
			warnxv(1, "Removed client: %d (%s/%s)",
			    cli->pid, cli->argv0, cli->uname);
		} else
			warnxv(1, "Removed spectator");
	}

	free(cli);
	return;
}

void
notifycb(int fd, short ev, void *data)
{
	client_printrates();
	evtimer_add(&notifyev, &notifytv);
}

void
gensigcb(int sig, short ev, void *data)
{
	char *sigstr;

	switch (sig) {
	case SIGTERM:
		sigstr = "SIGTERM";
		break;
	case SIGINT:
		sigstr = "SIGINT";
		break;
	default:
		sigstr = "an unknown signal";
		break;
	}

	cleanup_cleanup(cleanup);
	errxv(0, 0, "Received %s; quitting", sigstr);
}

void
usage(void)
{
	fprintf(stderr,
	    "Usage: %s [-hvVfs] [-d <rate>] [-u <rate>] [-t <seconds>] "
	    "[-l <length>]\n"
	    "       %*c [-p <priority>] [-c <file>] [-n <path>] [-N <seconds>]\n"
	    "       %*c [-w <length>]\n"
	    "\t-h            Help (this)\n"
	    "\t-v            Increase verbosity level\n"
	    "\t-V            Print %s version\n"
	    "\t-f            Run %s in the foreground\n"
	    "\t-s            Use syslog instead of stderr to print messages\n"
	    "\t-d <rate>     Set maximum cumulative download rate to <rate> KB/s\n"
	    "\t-u <rate>     Set maximum cumulative upload rate to <rate> KB/s\n"
	    "\t-t <seconds>  Set default smoothing time to <seconds> s\n"
	    "\t-l <length>   Set default smoothing length to <length> KB\n"
	    "\t-p <priority> Set default priority to <priority>\n"
	    "\t-c <file>     Use configuration file <file>\n"
	    "\t-n <path>     Set socket name to <path>\n"
	    "\t-N <seconds>  Notify of bandwidth usage every <seconds> s\n"
	    "\t-w <length>   Set window size to <length> s\n",
	    __progname, (int)strlen(__progname), ' ',
	    (int)strlen(__progname), ' ', __progname, __progname);

	exit(1);
}
