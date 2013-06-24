/*
 * print.h
 *
 * Copyright (c) 2001, 2002 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id: print.h,v 1.2 2003/02/27 14:13:25 marius Exp $
 */

#ifndef PRINT_H
#define PRINT_H

void print_setup(int, int);
void errv(int, int, const char *, ...);
void errxv(int, int, const char *, ...);
void warnv(int, const char *, ...);
void warnxv(int, const char *, ...);

#endif /* PRINT_H */
