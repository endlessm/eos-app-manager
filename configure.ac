AC_INIT([EndlessOS Application Manager],[0.1],[missing bug tracker],[eos-app-manager])

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

AC_PROG_CC_C_O
AC_PROG_CC_C99

# Compiler warnings
eam_ADD_CFLAGS([-Wall])
eam_ADD_CFLAGS([-Werror=logical-op])
eam_ADD_CFLAGS([-Werror=cast-align])
eam_ADD_CFLAGS([-Werror=shadow])
eam_ADD_CFLAGS([-Werror=format=2])
eam_ADD_CFLAGS([-Werror=init-self])
eam_ADD_CFLAGS([-Werror=empty-body])
eam_ADD_CFLAGS([-Werror=implicit-function-declaration])
eam_ADD_CFLAGS([-Werror=missing-declarations])
eam_ADD_CFLAGS([-Werror=redundant-decls])

IT_PROG_INTLTOOL([0.50.0])

PKG_PROG_PKG_CONFIG

GLIB_TESTS

GLIB_REQUIRED=2.40.0
SOUP_REQUIRED=2.44.0
JSON_GLIB_REQUIRED=0.16.2
POLKIT_GOBJECT_REQUIRED=0.110
PKG_CHECK_MODULES([DEPENDENCIES], [
		  glib-2.0 >= $GLIB_REQUIRED
		  gio-unix-2.0 >= $GLIB_REQUIRED
		  libsoup-2.4 >= $SOUP_REQUIRED
		  json-glib-1.0 >= $JSON_GLIB_REQUIRED
		  systemd
		  polkit-gobject-1 >= $POLKIT_GOBJECT_REQUIRED
		  ])

# GResource
AC_PATH_PROG([GLIB_COMPILE_RESOURCES], [glib-compile-resources], [no])
if test x$GLIB_COMPILE_RESOURCES = xno; then
   AC_MSG_ERROR(["Could not find a glib-compile-resources in your PATH"])
fi

# systemd
AC_SUBST([systemdsystemunitdir], [$($PKG_CONFIG --variable=systemdsystemunitdir systemd)])

GETTEXT_PACKAGE=eam
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])
AM_GLIB_GNU_GETTEXT

EAM_USER=app-manager
AC_SUBST(EAM_USER)

EAM_ADMIN_GROUP=adm
AC_SUBST(EAM_ADMIN_GROUP)
AC_DEFINE_UNQUOTED([EAM_ADMIN_GROUP_NAME],["$EAM_ADMIN_GROUP"],[System group with administration permissions])

# define the EOS architecture
AC_ARG_WITH([eos-arch],
    [AS_HELP_STRING([--with-eos-arch=ARCH], [specify the app architecture])],
    [EOS_ARCH=$with_eos_arch])

AC_ARG_ENABLE([debug],
              [AS_HELP_STRING([--enable-debug], [Enable debug messages])],
              [enable_debug=$enableval],
              [enable_debug=yes])

AS_IF([test "x$enable_debug" = xyes], [AC_DEFINE([EAM_ENABLE_DEBUG], [1], [Enable debug code])])

# Try to determine the debian architecture from the GNU host triplet.
# This is mostly borrowed from the dpkg tables in /usr/share/dpkg and
# only works for a few architectures that Endless might care about.
if test "x$with_eos_arch" = x; then
    # Start by looking at OS to determine if we're using armel or armhf.
    case "$host_os" in
    linux-gnueabihf)
        EOS_ARCH=armhf
        ;;
    linux-gnueabi)
        EOS_ARCH=armel
        ;;
    esac

    # Otherwise, map from the CPU type.
    case "$host_cpu" in
    i[3456]86)
        EOS_ARCH=i386
        ;;
    x86_64)
        EOS_ARCH=amd64
        ;;
    esac

    # Fallback to just using the GNU cpu and hoping it matches.
    test "x$EOS_ARCH" = x && EOS_ARCH=$host_cpu
fi
AC_SUBST([EOS_ARCH])
AC_DEFINE_UNQUOTED([EOS_ARCH], ["$EOS_ARCH"], [EOS app architecture])

AC_CONFIG_FILES([
Makefile
data/Makefile
src/Makefile
tools/Makefile
tests/Makefile
scripts/Makefile
po/Makefile.in
],
[])

AC_OUTPUT
