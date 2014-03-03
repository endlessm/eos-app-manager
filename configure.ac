AC_INIT([EndlessOS Application Manager],[0.1],[missing bug tracker],[eos-appman])

AC_PREREQ([2.59])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])

AM_INIT_AUTOMAKE([1.11 foreign dist-xz no-dist-gzip])

# Use AM_SILENT_RULES if present
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_MAINTAINER_MODE([enable])

# Initialize libtool
LT_PREREQ(2.2)
LT_INIT([disable-static])

AC_PROG_CC

IT_PROG_INTLTOOL([0.50.0])

PKG_PROG_PKG_CONFIG

GLIB_TESTS

# GResource
AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)

GLIB_REQUIRED=2.38.0
PKG_CHECK_MODULES([DEPENDENCIES], [
		  glib-2.0 >= $GLIB_REQUIRED
		  gio-unix-2.0 >= $GLIB_REQUIRED
		  ])

GETTEXT_PACKAGE=eam
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])
AM_GLIB_GNU_GETTEXT

AC_CONFIG_FILES([
Makefile
data/Makefile
src/Makefile
tests/Makefile
po/Makefile.in
],
[])

AC_OUTPUT
