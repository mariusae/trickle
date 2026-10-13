include $(top_srcdir)/Makefile.am.inc

TRICKLEOVERLOADFILES = @LTLIBOBJS@ trickle-overload.lo bwstat.lo trickledu.lo \
                       atomicio.lo xdr.lo
trickle-overload.so: $(TRICKLEOVERLOADFILES)
	$(LIBTOOL) --mode=link $(CC) $(CFLAGS) -o libtmp$@.la                \
		-rpath $(libdir)/trickle $(TRICKLEOVERLOADFILES) $(LIBS)     \
		$(LDFLAGS) -module -avoid-version -nostdlib @LIBDL@ @TIRPCLIB@
	@rm -f libtmp$@.la tmp$@.so* $@
	@cp .libs/libtmp$@.so $@
	@rm -f .libs/libtmp$@.*

trickleoverloaddir = $(libdir)/trickle
trickleoverload_DATA = trickle-overload.so

man_MANS = trickle.1 trickled.8 trickled.conf.5

bin_PROGRAMS = trickle trickled tricklectl

trickle_DEPENDENCIES = @ERRO@ $(LIBOBJS)
trickle_SOURCES = trickle.c util.c getopt.c
trickle_LDADD = @ERRO@ $(LIBOBJS)

trickled_SOURCES = trickled.c atomicio.c print.c bwstat.c client.c conf.c \
                   util.c cleanup.c getopt.c xdr.c
trickled_LDADD = @EVENTLIB@ @TIRPCLIB@ $(LIBOBJS)

tricklectl_SOURCES = tricklectl.c trickledu.c atomicio.c xdr.c
tricklectl_LDADD = @ERRO@ @TIRPCLIB@ $(LIBOBJS)

AM_CFLAGS = -Wall -Icompat @EVENTINC@ @TIRPCINC@

overloaddir = $(libdir)
overload_DATA = libtrickle.so

libtrickle.so: trickle-overload.c atomicio.c
$(overload_DATA):

CLEANFILES = *.so

# Test suite (make check). Optional: builds only when cmocka was found
# at configure time, so its absence does not block the normal build.
if HAVE_CMOCKA
check_PROGRAMS = tests/test_bwstat tests/test_atomicio tests/test_xdr
TESTS = $(check_PROGRAMS)

tests_test_bwstat_SOURCES = tests/test_bwstat.c bwstat.c
tests_test_bwstat_CPPFLAGS = -I$(top_srcdir) $(CMOCKAINC)
tests_test_bwstat_LDADD = $(CMOCKALIB)

tests_test_atomicio_SOURCES = tests/test_atomicio.c atomicio.c
tests_test_atomicio_CPPFLAGS = -I$(top_srcdir) $(CMOCKAINC)
tests_test_atomicio_LDADD = $(CMOCKALIB)

tests_test_xdr_SOURCES = tests/test_xdr.c xdr.c
tests_test_xdr_CPPFLAGS = -I$(top_srcdir) $(CMOCKAINC) @TIRPCINC@
tests_test_xdr_LDADD = $(CMOCKALIB) @TIRPCLIB@
endif

EXTRA_DIST = LICENSE README strlcat.c strlcpy.c err.c Makefile.am.inc	\
             compat/sys/queue.h compat/sys/tree.h trickle.1 trickled.8	\
	     trickled.conf.5 trickle-overload.c trickledu.c cleanup.h	\
	     conf.h message.h trickle.h util.h bwstat.h client.h	\
	     print.h trickledu.h xdr.h tests/test_bwstat.c		\
	     tests/test_atomicio.c tests/test_xdr.c

ACLOCAL_AMFLAGS = -I m4
