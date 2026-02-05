SUBDIRS = . tests examples

ACLOCAL_AMFLAGS = -I m4
AM_MAKEFLAGS = -s

lib_LTLIBRARIES = libdavici.la

# current:revision:age, do all of the following for each release:
#  source changes: revision++
#  interface added, removed, or changed: current++, revision = 0
#  interfaces added: age++
#  interfaces removed: age = 0
libdavici_la_LDFLAGS = -version-info 2:0:2

libdavici_la_SOURCES = \
	davici.c

nobase_include_HEADERS = \
	davici.h

AM_CFLAGS = -Wall -Wsign-compare

clean-local: cov-reset
	@find $(top_builddir) -name "*.gcno" -delete
	rm -rf $(top_builddir)/api

cov-reset:
	@rm -rf $(top_builddir)/coverage
	@find $(top_builddir) -name "*.gcda" -delete
	@lcov --directory $(top_builddir) --zerocounters || true

cov-makereport: cov-reset check
	@mkdir $(top_builddir)/coverage
	lcov -c -o $(top_builddir)/coverage/coverage.info -d $(top_builddir) \
			 --rc lcov_branch_coverage=1
	lcov -r $(top_builddir)/coverage/coverage.info '*/tests/*' \
			 -r $(top_builddir)/coverage/coverage.info '/usr/include/*' \
			 -o $(top_builddir)/coverage/coverage.cleaned.info \
			 --rc lcov_branch_coverage=1
	genhtml --num-spaces 4 --legend --branch-coverage --ignore-errors source \
			-t "$(PACKAGE_STRING)" -o $(top_builddir)/coverage/html \
			-p $$(readlink -m $(abs_top_srcdir)) \
			$(top_builddir)/coverage/coverage.cleaned.info

cov-report: cov-makereport
	xdg-open $(top_builddir)/coverage/html/index.html

doxygen: Doxyfile
	@test -d $(top_builddir)/api || doxygen
	@! find Doxyfile $(top_srcdir) \
		\( -name '*.h' -o -name '*.md' \) -newer $(top_builddir)/api | \
		grep -q '' || doxygen && touch $(top_builddir)/api

Doxyfile:	Doxyfile.in
	$(AM_V_GEN) \
	sed \
	-e "s:\@PACKAGE_VERSION\@:$(PACKAGE_VERSION):" \
	-e "s:\@PACKAGE_NAME\@:$(PACKAGE_NAME):" \
	-e "s:\@SRC_DIR\@:$(srcdir)/README.md $(srcdir)/davici.h:g" \
	$(srcdir)/$@.in > $@

CLEANFILES = Doxyfile

EXTRA_DIST = Doxyfile.in
