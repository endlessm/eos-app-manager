AC_INIT([EndlessOS Application Manager],
        [0.2],
        [https://github.com/endlessm/eos-shell/issues],
        [eos-app-manager],
        [https://endlessm.com])

AC_PREREQ([2.63])

AC_CONFIG_MACRO_DIR([build/autotools])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_HEADER([src/config.h])
AC_CONFIG_SRCDIR([src/main.c])

ACLOCAL="$ACLOCAL $ACLOCAL_FLAGS"

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

AM_INIT_AUTOMAKE([1.11.2 no-define -Wno-portability subdir-objects foreign tar-ustar dist-xz no-dist-gzip])
AM_SILENT_RULES([yes])
AM_MAINTAINER_MODE([enable])

# Initialize libtool
LT_PREREQ(2.2)
LT_INIT([disable-static])

AC_PROG_CC_C_O
AC_PROG_CC_C99
AC_PROG_SED

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

PKG_PROG_PKG_CONFIG

GLIB_TESTS

# Dependencies
GLIB_REQUIRED=2.44.0
POLKIT_GOBJECT_REQUIRED=0.110
LIBARCHIVE_REQUIRED=3.1.2
LIBSOUP_REQUIRED=2.52
PKG_CHECK_MODULES([DEPENDENCIES],
                  [
                    glib-2.0 >= $GLIB_REQUIRED
                    gio-unix-2.0 >= $GLIB_REQUIRED
                    polkit-gobject-1 >= $POLKIT_GOBJECT_REQUIRED
                    libarchive >= $LIBARCHIVE_REQUIRED
                    libsoup-2.4 >= $LIBSOUP_REQUIRED
                    libsystemd
                    systemd
                  ])

# GResource
GLIB_COMPILE_RESOURCES=`$PKG_CONFIG --variable=glib_compile_resources gio-2.0`
AC_SUBST(GLIB_COMPILE_RESOURCES)

# GDBus code generator
GDBUS_CODEGEN=`$PKG_CONFIG --variable gdbus_codegen gio-2.0`
AC_SUBST(GDBUS_CODEGEN)

# systemd
AC_ARG_WITH([systemd-systemunitdir],
            [AS_HELP_STRING([--with-systemd-systemunitdir], [Path to the system directory for systemd units])],
            [systemunitdir="$withval"],
            [systemunitdir=default])

AS_IF([test "x$systemunitdir" = xdefault],
      [
        systemdsystemunitdir=`$PKG_CONFIG --variable=systemdsystemunitdir systemd`
      ],
      [ systemdsystemunitdir="$systemunitdir" ])

AC_SUBST([systemdsystemunitdir])

# Localization
AM_GNU_GETTEXT_VERSION([0.18])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE="eam"
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])

AC_ARG_WITH([eam-user],
            [AS_HELP_STRING([--with-eam-user], [The app manager username (DEFAULT: app-manager)])],
            [eam_user=$withval],
            [eam_user=app-manager])

EAM_USER="$eam_user"
AC_DEFINE_UNQUOTED([EAM_USER_NAME], ["$EAM_USER"], [System user for the app manager])
AC_SUBST(EAM_USER)

AC_ARG_WITH([eam-admin-group],
            [AS_HELP_STRING([--with-eam-admin-group], [Privileged admin groupname (DEFAULT: adm)])],
            [eam_admin_group=$withval],
            [eam_admin_group=adm])

EAM_ADMIN_GROUP="$eam_admin_group"
AC_DEFINE_UNQUOTED([EAM_ADMIN_GROUP_NAME],["$EAM_ADMIN_GROUP"],[System group with administration permissions])
AC_SUBST(EAM_ADMIN_GROUP)

EAM_CFLAGS="$DEPENDENCIES_CFLAGS"
EAM_LIBS="$DEPENDENCIES_LIBS"
AC_SUBST(EAM_CFLAGS)
AC_SUBST(EAM_LIBS)

AC_CONFIG_FILES([
        Makefile
        data/Makefile
        src/Makefile
        tools/Makefile
        tests/Makefile
        po/Makefile.in
], [])

AC_OUTPUT
