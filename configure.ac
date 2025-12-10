dnl $Id: configure.in,v 1.22 2004/02/13 06:14:01 marius Exp $
AC_INIT([trickle],[1.07])
AC_CONFIG_SRCDIR([trickle.c])

AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC

LT_INIT([dlopen])
AC_PROG_MAKE_SET

dnl intitialization
if test "x$prefix" = "xNONE"; then
    prefix="/usr/local"
fi

dnl Checks for programs.

AC_PROG_RANLIB
AC_PROG_INSTALL

dnl Platform specific stuff
case "$host" in
*-*-hpux*)
    AC_DEFINE(SPT_TYPE,SPT_PSTAT)
    LIBS="$LIBS -lxnet -lsec"
    ;;
esac

dnl Checks for standard libraries.
AC_CHECK_LIB(socket, socket)
AC_CHECK_LIB(nsl, gethostbyname)
AC_CHECK_LIB(dl, dlopen, [ LIBDL="-ldl" 
			   LIBS="$LIBS -ldl" ],,)
AC_SUBST(LIBDL)
dnl ugly ugly hack	
AC_CHECK_LIB(c, err, [ ERRO="" ], [ ERRO="err.o" ],)
AC_SUBST(ERRO)

AC_HEADER_SYS_WAIT

dnl Checks for pthread
threadsafe="yes"

AC_ARG_ENABLE([threadsafe],
	[AS_HELP_STRING([--disable-threadsafe],[disable trickle thread safety (default enabled)])],
	[threadsafe=$enableval]
	)

if test "x$threadsafe" = xyes; then
	AX_PTHREAD([AC_DEFINE([HAVE_PTHREAD],[1],[Define if you have POSIX threads libraries and header files.])],
	[AC_MSG_ERROR(cannot find pthread. use configure script option --disable-threadsafe to compile trickle without thread safety)])
fi

dnl Check for __progname; from OpenSSHp
AC_CACHE_CHECK([if libc defines __progname], ac_cv_libc_defines___progname, [   
        AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[ extern char *__progname; printf("%s", __progname); ]])],[ ac_cv_libc_defines___progname="yes" ],[ ac_cv_libc_defines___progname="no" 
        ])
])
if test "x$ac_cv_libc_defines___progname" = "xyes" ; then
   AC_DEFINE(HAVE___PROGNAME)
fi

AC_CHECK_HEADERS(sys/ioctl.h sys/time.h err.h time.h unistd.h stdint.h netinet/in.h asm/poll.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
m4_warn([obsolete],
[Update your code to rely only on HAVE_SYS_TIME_H,
then remove this warning and the obsolete code below it.
All current systems provide time.h; it need not be checked for.
Not all systems provide sys/time.h, but those that do, all allow
you to include it and time.h simultaneously.])dnl
AC_CHECK_HEADERS_ONCE([sys/time.h])
# Obsolete code to be removed.
if test $ac_cv_header_sys_time_h = yes; then
  AC_DEFINE([TIME_WITH_SYS_TIME],[1],[Define to 1 if you can safely include both <sys/time.h>
	     and <time.h>.  This macro is obsolete.])
fi
# End of obsolete code.

AC_CHECK_TYPE(u_int64_t, unsigned long long)
AC_CHECK_TYPE(u_int32_t, unsigned int)
AC_CHECK_TYPE(u_int16_t, unsigned short)
AC_CHECK_TYPE(u_int8_t, unsigned char)
AC_CHECK_TYPE(in_addr_t, [], [AC_DEFINE([in_addr_t],[unsigned long],[])], [#include <netinet/in.h>])

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL
m4_warn([obsolete],
[your code may safely assume C89 semantics that RETSIGTYPE is void.
Remove this warning and the `AC_CACHE_CHECK' when you adjust the code.])dnl
AC_CACHE_CHECK([return type of signal handlers],[ac_cv_type_signal],[AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([#include <sys/types.h>
#include <signal.h>
],
		 [return *(signal (0, 0)) (0) == 1;])],
		   [ac_cv_type_signal=int],
		   [ac_cv_type_signal=void])])
AC_DEFINE_UNQUOTED([RETSIGTYPE],[$ac_cv_type_signal],[Define as the return type of signal handlers
		    (`int' or `void').])

AC_CHECK_FUNCS(socket)
AC_REPLACE_FUNCS(strlcat strlcpy daemon setenv strsep)

dnl XXX
AC_CHECK_FUNCS(sendfile)

dnl Checks for libevent
AC_MSG_CHECKING(for libevent)
AC_ARG_WITH(libevent,
[  --with-libevent=DIR     use libevent in DIR],
[ case "$withval" in
  yes|no)
     AC_MSG_RESULT(no)
     ;;
  *)
     AC_MSG_RESULT($withval)
     if test -f $withval/include/event.h -a -f $withval/lib/libevent.a; then
        owd=`pwd`
        if cd $withval; then withval=`pwd`; cd $owd; fi
        EVENTINC="-I$withval/include"
        EVENTLIB="-L$withval/lib -levent"
     elif test -f $withval/event.h -a -f $withval/libevent.a; then
        owd=`pwd`
        if cd $withval; then withval=`pwd`; cd $owd; fi
        EVENTINC="-I$withval"
        EVENTLIB="-L$withval -levent"
     else
        AC_MSG_ERROR(event.h or libevent.a not found in $withval)
     fi
     ;;
  esac ],
[ if test -f ${prefix}/include/event.h -a -f ${prefix}/lib/libevent.a;
then
     EVENTINC="-I${prefix}/include"
     EVENTLIB="-L${prefix}/lib -levent"
  elif test -f /usr/include/event.h; then
     EVENTLIB="-levent"
  else
     AC_MSG_RESULT(no)
     AC_MSG_ERROR(libevent not found)
  fi
  AC_MSG_RESULT(yes) ]
)
AC_SUBST(EVENTINC)
AC_SUBST(EVENTLIB)

dnl check if underscores are needed
AC_MSG_CHECKING(if underscores are needed for symbols)
AC_RUN_IFELSE([AC_LANG_SOURCE([[#include <dlfcn.h>
#include <stdio.h>

int underscoreprobe(void) { return (31415); }

int
main(int argc, char **argv)
{
	void *dh;

	if ((dh = dlopen(NULL, 0)) == NULL)
		exit(1);

	if (dlsym(dh, "_underscoreprobe") != NULL)
		exit(0);

	exit(1);	
}]])],[AC_MSG_RESULT(yes)
   AC_DEFINE(DL_NEED_UNDERSCORE)],[AC_MSG_RESULT(no)],[AC_MSG_RESULT(no)])

HAVEMETHOD=no

AC_MSG_CHECKING(if we can access libc without dlopen)
AC_RUN_IFELSE([AC_LANG_SOURCE([[#include <dlfcn.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
	void *dh;

	dh = (void *)-1L;

	if (dlsym(dh, "printf") != NULL || dlsym(dh, "_printf") != NULL)
		exit(0);

	exit(1);
}]])],[AC_MSG_RESULT(yes)
    AC_DEFINE(NODLOPEN)
    HAVEMETHOD=yes],[AC_MSG_RESULT(no)],[AC_MSG_RESULT(no)])

if test "$HAVEMETHOD" = "no"; then
    dnl ugly, ugly hack		     
    LIBCGUESS=`echo /lib/libc.so.*`
    USRLIBCGUESS=`echo /usr/lib/libc.so*`
    
    if test "$USRLIBCGUESS" = "/usr/lib/libc.so.*"; then
       USRLIBCGUESS=""
    fi
    
    if test "$LIBCGUESS" = "/lib/libc.so.*"; then
       LIBCGUESS=""
    fi
    
    for TESTLIB in libc.so `echo $USRLIBCGUESS` `echo $LIBCGUESS`
    do
        AC_MSG_CHECKING(if we can access libc with $TESTLIB)
        AC_RUN_IFELSE([AC_LANG_SOURCE([[#include <dlfcn.h>
        #include <stdio.h>
    
        int
        main(int argc, char **argv)
        {
    	    void *dh;
    
    	    if (dlopen("$TESTLIB", RTLD_LAZY) != NULL)
    	           exit(0);
    
            exit(1);
        }]])],[AC_MSG_RESULT(yes)
            AC_DEFINE_UNQUOTED(DLOPENLIBC, "$TESTLIB")
	    HAVEMETHOD=yes],[AC_MSG_RESULT(no)],[AC_MSG_RESULT(no)])

	if test "$HAVEMETHOD" = "yes"; then
	   break
	fi
    done
fi

if test "$HAVEMETHOD" = "no"; then
   AC_MSG_ERROR(cannot figure out how to access libc)
fi

if test "x$sysconfdir" = "x$prefix/etc"; then
    csysconfdir="$prefix/etc"
else
    csysconfdir="$sysconfdir"
fi

clibdir="$prefix/lib/trickle"

AC_DEFINE_UNQUOTED(SYSCONFDIR, "$csysconfdir")
AC_DEFINE_UNQUOTED(LIBDIR, "$clibdir")

havesendfile=no
AC_CHECK_FUNCS(sendfile, [havesendfile=yes], )
if test "$havesendfile" = "yes"; then
   AC_DEFINE(HAVE_SENDFILE, 1, [Define if your system has the sendfile system call])
fi

RPCINC="-I${prefix}/include/tirpc"
RPCLIB="-L${prefix}/lib -ltirpc -lc"
AC_SUBST(RPCINC)
AC_SUBST(RPCLIB)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

