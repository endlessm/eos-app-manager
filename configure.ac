AC_INIT([EndlessOS Application Manager],[0.1],[missing bug tracker],[eos-appman])

AC_PREREQ([2.59])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])

AM_INIT_AUTOMAKE([1.11 foreign dist-xz no-dist-gzip tar-ustar])

# Use AM_SILENT_RULES if present
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_MAINTAINER_MODE([enable])

# Initialize libtool
LT_PREREQ(2.2)
LT_INIT([disable-static])

AC_PROG_CC

PKG_PROG_PKG_CONFIG

GLIB_REQUIRED=2.38.0
JSON_GLIB_REQUIRED=0.16.0

PKG_CHECK_MODULES([DEPENDENCIES], [
		  glib-2.0 >= $GLIB_REQUIRED
		  gio-unix-2.0 >= $GLIB_REQUIRED
		  json-glib-1.0 >= $JSON_GLIB_REQUIRED
		  ])

AC_CONFIG_FILES([
Makefile
src/Makefile
],
[])

AC_OUTPUT
