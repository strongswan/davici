AM_CPPFLAGS = \
	-I$(top_srcdir)

AM_CFLAGS = -Wall

check_LTLIBRARIES = libtester.la

libtester_la_SOURCES = \
	tester.c tester.h

LDADD = \
	$(top_builddir)/libdavici.la \
	libtester.la

TESTS = \
	cmd.tst \
	dump.tst \
	many.tst \
	event.tst \
	stream.tst \
	tcpsock.tst \
	recurse.tst \
	badsock.tst \
	cmdunknown.tst \
	badtcpsock.tst \
	eventunknown.tst

cmd_tst_SOURCES = cmd.c
dump_tst_SOURCES = dump.c
many_tst_SOURCES = many.c
event_tst_SOURCES = event.c
stream_tst_SOURCES = stream.c
recurse_tst_SOURCES = recurse.c
badsock_tst_SOURCES = badsock.c
badtcpsock_tst_SOURCES = badtcpsock.c
tcpsock_tst_SOURCES = tcpsock.c
cmdunknown_tst_SOURCES = cmdunknown.c
eventunknown_tst_SOURCES = eventunknown.c

check_PROGRAMS = $(TESTS)
