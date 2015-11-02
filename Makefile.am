ACLOCAL_AMFLAGS = -I m4
AM_MAKEFLAGS = -s

lib_LTLIBRARIES = libdavici.la

# current:revision:age, do all of the following for each release:
#  source changes: revision++
#  interface added, removed, or changed: current++, revision = 0
#  interfaces added: age++
#  interfaces removed: age = 0
libdavici_la_LDFLAGS = -version-info 0:0:0

libdavici_la_SOURCES = \
	davici.c davici.h

AM_CFLAGS = -Wall
