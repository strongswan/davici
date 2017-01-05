AC_PREREQ([2.69])
AC_INIT([davici], [1.2])
AC_CONFIG_SRCDIR([tests])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([tar-ustar subdir-objects])
AM_SILENT_RULES([yes])

LT_INIT
AC_PROG_CC

AC_CHECK_FUNCS(fmemopen funopen)

AC_CONFIG_FILES([
	Makefile
	tests/Makefile
])
AC_OUTPUT
