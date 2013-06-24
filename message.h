/*
 * message.h
 *
 * Copyright (c) 2003 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id: message.h,v 1.6 2003/05/02 06:33:28 marius Exp $
 */

#ifndef TRICKLE_MESSAGE_H
#define TRICKLE_MESSAGE_H

/* XXX */
#define SOCKNAME "trickle.sock"

#define MSG_STATUS_FAIL 1

struct msg_conf {
	uint   lim[2];
	pid_t  pid;
	char   argv0[256];
	uid_t  uid;
	gid_t  gid;
};

struct msg_delay {
	ssize_t len;
	short   dir;
};

struct msg_update {
	size_t  len;
	short   dir;
};

struct msg_delayinfo {
	struct timeval delaytv;
	ssize_t        len;
};

struct msg_getinfo {
	struct {
		uint32_t lim;
		uint32_t rate;
	} dirinfo[2];
};

enum msgtype { MSG_TYPE_NEW, MSG_TYPE_CONF, MSG_TYPE_UPDATE, MSG_TYPE_CONT,
	       MSG_TYPE_DELAY, MSG_TYPE_GETDELAY, MSG_TYPE_DELAYINFO,
	       MSG_TYPE_SPECTATOR, MSG_TYPE_GETINFO };

struct msg {
	enum msgtype type;
	short        status;
	union {
		struct msg_conf      conf;
		struct msg_delay     delay;
		struct msg_update    update;
		struct msg_delayinfo delayinfo;
		struct msg_getinfo   getinfo;
	} data;
};

#endif /* TRICKLE_MESSAGE_H */
