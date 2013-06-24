include $(top_srcdir)/Makefile.am.inc

TRICKLEOVERLOADFILES = @LTLIBOBJS@ trickle-overload.lo bwstat.lo trickledu.lo \
                       atomicio.lo xdr.lo
trickle-overload.so: $(TRICKLEOVERLOADFILES)
	$(LIBTOOL) --mode=link $(CC) $(CFLAGS) -o libtmp$@.la                \
		-rpath $(libdir)/trickle $(TRICKLEOVERLOADFILES) $(LIBS)     \
		$(LDFLAGS) -module -avoid-version -nostdlib @LIBDL@
	@rm -f libtmp$@.la tmp$@.so* $@
	@cp .libs/libtmp$@.so $@
	@rm -f .libs/libtmp$@.*

trickleoverloaddir = $(libdir)/trickle
trickleoverload_DATA = trickle-overload.so

man_MANS = trickle.1 trickled.8 trickled.conf.5

bin_PROGRAMS = trickle trickled tricklectl

trickle_DEPENDENCIES = @ERRO@ @LIBOBJS@
trickle_SOURCES = trickle.c util.c getopt.c
trickle_LDADD = @ERRO@ @LIBOBJS@

trickled_SOURCES = trickled.c atomicio.c print.c bwstat.c client.c conf.c \
                   util.c cleanup.c getopt.c xdr.c
trickled_LDADD = @EVENTLIB@ @LIBOBJS@

tricklectl_SOURCES = tricklectl.c trickledu.c atomicio.c xdr.c
tricklectl_LDADD = @ERRO@ @LIBOBJS@

AM_CFLAGS += -Wall -Icompat @EVENTINC@

overloaddir = $(libdir)
overload_DATA = libtrickle.so

libtrickle.so: trickle-overload.c atomicio.c
$(overload_DATA):

CLEANFILES = *.so

EXTRA_DIST = LICENSE README strlcat.c strlcpy.c err.c Makefile.am.inc	\
             compat/sys/queue.h compat/sys/tree.h trickle.1 trickled.8	\
	     trickled.conf.5 trickle-overload.c trickledu.c cleanup.h	\
	     conf.h message.h trickle.h util.h bwstat.h client.h	\
	     print.h trickledu.h xdr.h
