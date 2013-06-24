/*
 * xdr.h
 *
 * Copyright (c) 2003 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id: xdr.h,v 1.1 2003/05/02 06:33:28 marius Exp $
 */

#ifndef TRICKLE_XDR_H
#define TRICKLE_XDR_H

int xdr2msg(struct msg *, u_char *, uint32_t);
int msg2xdr(struct msg *, u_char *, uint32_t *);

#endif /* TRICKLE_XDR_H */
