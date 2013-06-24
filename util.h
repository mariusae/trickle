/*
 * trickledu.h
 *
 * Copyright (c) 2003 Marius Aamodt Eriksen <marius@monkey.org>
 * All rights reserved.
 *
 * $Id: util.h,v 1.3 2003/03/07 09:35:18 marius Exp $
 */

#ifndef TRICKLE_UTIL_H
#define TRICKLE_UTIL_H

#ifndef TIMEVAL_TO_TIMESPEC
#define TIMEVAL_TO_TIMESPEC(tv, ts) {                                   \
        (ts)->tv_sec = (tv)->tv_sec;                                    \
        (ts)->tv_nsec = (tv)->tv_usec * 1000;                           \
}
#endif /* !TIMEVAL_TO_TIMESPEC */

#ifndef timersub
#define timersub(a, b, result)                                  \
   do {                                                         \
      (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;             \
      (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;          \
      if ((result)->tv_usec < 0) {                              \
         --(result)->tv_sec;                                    \
         (result)->tv_usec += 1000000;                          \
      }                                                         \
   } while (0)
#endif

#undef SET
#undef CLR
#undef ISSET
#define SET(t, f)       ((t) |= (f))
#define CLR(t, f)       ((t) &= ~(f))
#define ISSET(t, f)     ((t) & (f))

#undef MAX
#undef MIN
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

ssize_t    atomicio(ssize_t (*)(), int, void *, size_t);
char      *get_progname(char *);


#endif /* TRICKLE_UTIL_H */
