#undef socklen_t
#undef u_int16_t
#undef u_int32_t
#undef u_int64_t
#undef u_int8_t

#undef in_addr_t

#undef SYSCONFDIR
#undef LIBDIR

#undef SPT_TYPE
#undef HAVE___PROGNAME
#undef DL_NEED_UNDERSCORE
#undef NODLOPEN
#undef DLOPENLIBC

@BOTTOM@

/* Prototypes for missing functions */
#ifndef HAVE_STRLCAT
size_t	 strlcat(char *, const char *, size_t);
#endif

#ifndef HAVE_STRLCPY
size_t	 strlcpy(char *, const char *, size_t);
#endif

#ifndef HAVE_SETENV
int setenv(register const char *name, register const char *value, int rewrite);
#endif /* !HAVE_SETENV */

#ifndef HAVE_STRSEP
char *strsep(char **, const char *);
#endif /* HAVE_STRSEP */

#ifndef HAVE_ERR
void     err(int, const char *, ...);
void     warn(const char *, ...);
void     errx(int , const char *, ...);
void     warnx(const char *, ...);
#endif

#ifndef HAVE_DAEMON
int      daemon(int, int);
#endif /* HAVE_DAEMON */
