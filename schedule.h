/*
 * trickle-overload.c
 *
 * Copyright (c) 2002, 2003 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id: trickle-overload.c,v 1.38 2004/02/13 06:11:21 marius Exp $
 */

#ifndef SCHEDULE_H
#define SCHEDULE_H

uint   getSchedIndex ();
typedef void (*print_func)   (int, const char *, ...);
void   schedString   (char* sched,
                              uint* bwList,
                              const char* updown,
                              print_func print);

#endif /* !SCHEDULE_H */
