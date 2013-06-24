/*
 * xdr.c
 *
 * Copyright (c) 2003 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id: xdr.c,v 1.3 2003/06/01 18:39:12 marius Exp $
 */

#include <rpc/rpc.h>

#include "message.h"

#define X(x) do {				\
	if (!x)					\
		return (FALSE);			\
} while (0)

#if 0
#define X(x) do {				\
	if (!x)	{				\
		warnx("XDRERR: " #x);		\
		return (FALSE);			\
	} else {				\
		warnx("XDROK: " #x);		\
	}					\
} while (0)
#endif 

static int
xdr_msg_conf(XDR *xdrs, struct msg_conf *conf)
{
	X(xdr_u_int(xdrs, (u_int *)&conf->lim[0]));
	X(xdr_u_int(xdrs, (u_int *)&conf->lim[1]));
	X(xdr_int(xdrs, (int *)&conf->pid));
	X(xdr_opaque(xdrs, conf->argv0, sizeof(conf->argv0)));
	X(xdr_u_int(xdrs, (u_int *)&conf->uid));
	X(xdr_u_int(xdrs, (u_int *)&conf->gid));

	return (TRUE);
}

static int
xdr_msg_delay(XDR *xdrs, struct msg_delay *delay)
{
	X(xdr_int(xdrs, (int *)&delay->len));
	X(xdr_short(xdrs, &delay->dir));

	return (TRUE);
}

static int
xdr_msg_update(XDR *xdrs, struct msg_update *update)
{
	X(xdr_u_int(xdrs, (u_int *)&update->len));
	X(xdr_short(xdrs, &update->dir));

	return (TRUE);
}

static int
xdr_msg_delayinfo(XDR *xdrs, struct msg_delayinfo *delayinfo)
{
	X(xdr_long(xdrs, &delayinfo->delaytv.tv_sec));
	X(xdr_long(xdrs, &delayinfo->delaytv.tv_usec));
	X(xdr_int(xdrs, (int *)&delayinfo->len));

	return (TRUE);
}

static int
xdr_msg_getinfo(XDR *xdrs, struct msg_getinfo *getinfo)
{
	X(xdr_u_int(xdrs, (u_int *)&getinfo->dirinfo[0].lim));
	X(xdr_u_int(xdrs, (u_int *)&getinfo->dirinfo[0].rate));
	X(xdr_u_int(xdrs, (u_int *)&getinfo->dirinfo[1].lim));
	X(xdr_u_int(xdrs, (u_int *)&getinfo->dirinfo[1].rate));

	return (TRUE);
}

static struct xdr_discrim xdr_msg_discrim[] = {
	/* { MSG_TYPE_NEW, NULL }, */
	{ MSG_TYPE_CONF, (xdrproc_t)xdr_msg_conf },
	{ MSG_TYPE_UPDATE, (xdrproc_t)xdr_msg_update },
	{ MSG_TYPE_CONT, (xdrproc_t)xdr_msg_delayinfo },
	{ MSG_TYPE_DELAY, (xdrproc_t)xdr_msg_delay },
	{ MSG_TYPE_GETDELAY, (xdrproc_t)xdr_msg_delay },
	{ MSG_TYPE_DELAYINFO, (xdrproc_t)xdr_msg_delayinfo },
	/* { MSG_TYPE_SPECTATOR, NULL }, */
	{ MSG_TYPE_GETINFO, (xdrproc_t)xdr_msg_getinfo },
	{ __dontcare__, NULL }
};

static bool_t
_xdr_void(void)
{
	return (TRUE);
}

static int
xdr_msg(XDR *xdrs, struct msg *msg)
{
	X(xdr_short(xdrs, &msg->status));
	X(xdr_union(xdrs, (int *)&msg->type, (char *)&msg->data,
	      xdr_msg_discrim, _xdr_void));

	return (TRUE);
}

int
msg2xdr(struct msg *msg, u_char *buf, uint32_t *buflen)
{
	XDR xdrs;

	xdrmem_create(&xdrs, buf, *buflen, XDR_ENCODE);

	if (!xdr_msg(&xdrs, msg)) {
		xdr_destroy(&xdrs);
		return (-1);
	}

	*buflen = xdr_getpos(&xdrs);

	xdr_destroy(&xdrs);

	return (0);
}

int
xdr2msg(struct msg *msg, u_char *buf, uint32_t buflen)
{
	XDR xdrs;
	int ret = 0;

	xdrmem_create(&xdrs, buf, buflen, XDR_DECODE);

	if (!xdr_msg(&xdrs, msg))
		ret = -1;

	xdr_destroy(&xdrs);

	return (ret);
}
