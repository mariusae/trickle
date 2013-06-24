/*
 * util.c
 *
 * Copyright (c) 2001, 2002 Marius Aamodt Eriksen <marius@monkey.org>
 *
 * $Id: util.c,v 1.3 2003/03/06 05:49:36 marius Exp $
 */

#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>

/* From OpenSSH */

char *
get_progname(char *argv0)
{
#ifdef HAVE___PROGNAME
        extern char *__progname;

        return __progname;
#else
        char *p;

        if (argv0 == NULL)
                return "unknown";       /* XXX */
        p = strrchr(argv0, '/');
        if (p == NULL)
                p = argv0;
        else
                p++;
        return p;
#endif
}
