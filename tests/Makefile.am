AM_CPPFLAGS = \
	-I$(top_srcdir)

AM_CFLAGS = -Wall

noinst_LTLIBRARIES = libtester.la

libtester_la_SOURCES = \
	tester.c tester.h

LDADD = \
	$(top_builddir)/libdavici.la \
	libtester.la

TESTS = \
	cmd.tst \
	many.tst \
	event.tst \
	stream.tst

cmd_tst_SOURCES = cmd.c
many_tst_SOURCES = many.c
event_tst_SOURCES = event.c
stream_tst_SOURCES = stream.c

check_PROGRAMS = $(TESTS)
