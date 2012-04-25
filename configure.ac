#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_COPYRIGHT([Copyright (c) 2012 Varnish Software AS
Copyright (c) 2009-2012 Kristian Lyngstøl <kly@kly.no>])
AC_INIT([spew], [0.1],[kristian@bohemians.org])
AC_CONFIG_SRCDIR([config.h.in])
AM_CONFIG_HEADER(config.h)

AM_INIT_AUTOMAKE(spew, 0.1, [-Wall -Werror foreign])
AM_CFLAGS="-Wall -Werror"

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_CHECK_PROGS(TCLSH, [tclsh tclsh8.4 tclsh8.5], :)
if test "$TCLSH" = :; then
	TCLSH="${am_missing_run}tclsh"
fi

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([limits.h stdlib.h string.h sys/param.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([strcasecmp strerror strtol])

AC_CONFIG_FILES([Makefile
                 include/Makefile
                 src/Makefile])
AC_OUTPUT
