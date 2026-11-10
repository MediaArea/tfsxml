dnl #########################################################################
dnl ### Init
dnl #########################################################################

dnl -------------------------------------------------------------------------
dnl Name and version
dnl
AC_INIT([libtfsxml], [0.1.0])

dnl -------------------------------------------------------------------------
dnl Test if we are at the good place
dnl
AC_CONFIG_SRCDIR([../../../Source/tfsxml/tfsxml.c])

dnl -------------------------------------------------------------------------
dnl sets build, host, target variables and the same with _alias
dnl
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([foreign -Wall subdir-objects])

dnl -------------------------------------------------------------------------
dnl Autotools init
dnl
AC_PROG_CC
AC_PROG_INSTALL
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

dnl -------------------------------------------------------------------------
dnl Libtool init
dnl
case "$host_os" in
	darwin*)
		dnl libtool wrongly detects -single_module as unsupported with recent Xcode/Clang versions.
		lt_cv_apple_cc_single_mod=yes
		;;
esac

LT_INIT([win32-dll])

dnl #########################################################################
dnl ### Options
dnl #########################################################################

dnl -------------------------------------------------------------------------
dnl Enable
dnl
AC_ARG_ENABLE([debug],            [AS_HELP_STRING([--enable-debug],             [Enable full debug])],                          , [enable_debug=no])
AC_ARG_ENABLE([no_libc],          [AS_HELP_STRING([--enable-no_libc],           [Do not use the C library (no memchr)])],       , [enable_no_libc=no])

dnl #########################################################################
dnl ### C compiler options used to compile
dnl #########################################################################

dnl -------------------------------------------------------------------------
dnl No C library
dnl
if test "$enable_no_libc" = "yes"; then
	CFLAGS="$CFLAGS -DTFSXML_NO_LIBC"
fi

dnl -------------------------------------------------------------------------
dnl GNU compiler
dnl
if test "$GCC" = yes ; then
	if test "$enable_debug" = "yes"; then
		CFLAGS="$CFLAGS -O0 -g -Wall -Wextra -Wundef"
	else
		CFLAGS="$CFLAGS -O2"
	fi
fi

dnl #########################################################################
dnl ### Output
dnl #########################################################################

AC_CONFIG_FILES(libtfsxml.pc)
AC_CONFIG_FILES([Makefile])

AC_OUTPUT

dnl #########################################################################
dnl ### Report how we have been configured
dnl #########################################################################

echo ""
echo "Configured tfsxml for '$build'"
echo ""
echo "  Without C library?                                      $enable_no_libc"
echo ""
echo "  Create static lib?                                      $enable_static"
echo "  Create shared lib?                                      $enable_shared"
echo ""
echo "  CFLAGS: $CFLAGS"
echo "  LDFLAGS: $LDFLAGS"
