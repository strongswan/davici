AM_CPPFLAGS = \
	-I$(top_srcdir)

AM_CFLAGS = -Wall

noinst_LTLIBRARIES = libtester.la

libtester_la_SOURCES = \
	tester.c tester.h

LDADD = \
	$(top_builddir)/libdavici.la \
	libtester.la

TESTS =

check_PROGRAMS = $(TESTS)
