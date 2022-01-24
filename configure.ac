dnl  Process this file with autoconf to produce a configure script.

m4_include([autostuff/m4/not-autotools.m4])

AC_PREREQ([2.69])

AC_INIT([GNUnet Worker],
	[0.1.0],
	[madmurphy333@gmail.com],
	[lib]n4_mem([GL_PROJECT_NAME], [gnunetworker]),
	[https://github.com/madmurphy/libgnunetworker])

NC_GLOBAL_LITERALS(
	[INTERFACE_NUM],	[0],
	[IMPLEMENTATION_NUM],	[0],
	[INTERFACES_SUPPORTED],	[0],
	[PROJECT_DESCRIPTION],	[Multithreading with GNUnet],
)

NC_REQUIRE([AC_PROG_LN_S], [AC_PROG_SED])

AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_AUX_DIR([build-aux])

AC_CONFIG_SRCDIR([src/worker.c])

AC_CONFIG_HEADERS([config.h])

AC_CONFIG_FILES([Makefile
src/Makefile
src/]GL_PROJECT_NAME[.pc
po/Makefile.in])

AM_INIT_AUTOMAKE([1.11])

AM_SILENT_RULES([yes])

AC_PROG_CC

AC_PROG_GREP

AC_CHECK_PROG([HAVE_PKGCONFIG], [pkg-config], [yes], [no])

AS_IF([test "x${HAVE_PKGCONFIG}" = xno],
	[AC_MSG_ERROR([you need to have pkgconfig installed!])])

PKG_CHECK_MODULES([GNUNET_WORKER], [gnunetutil])

AC_ARG_VAR([FIND], [Unix find utility])

AS_IF([test "x${FIND}" = x], [AC_PATH_PROG([FIND], [find])])

AM_CONDITIONAL([HAVE_FIND], [test "x${FIND}" != x])

###  Keep the `pkgconfigdir` environment variable when non-empty
AS_IF([test "x${pkgconfigdir}" = x],
	[AS_VAR_SET([pkgconfigdir], ['$(libdir)/pkgconfig'])])

###  Add `--with-pkgconfigdir=DIR` option
AC_ARG_WITH([pkgconfigdir],
	[AS_HELP_STRING([--with-pkgconfigdir=DIR],
		[pkg-config installation directory
		@<:@default=<libdir>/pkgconfig@:>@])],
	[AS_IF([test "x${withval}" = x -o "x${with_pkgconfigdir}" = xyes],
		[AC_MSG_ERROR([a DIR value must be specified for option --with-pkgconfigdir=DIR])],
		[AS_IF([test "x${with_pkgconfigdir}" = xno],
			[AC_MSG_WARN([unrecognized option: --without-pkgconfigdir])],
			[AS_VAR_COPY([pkgconfigdir], [withval])])])],
	[:])

AC_SUBST([pkgconfigdir])

AM_GNU_GETTEXT([external])

AM_GNU_GETTEXT_VERSION([0.19])

AM_CONDITIONAL([NLS_ENABLED], [test "x${USE_NLS}" != xno])

AM_COND_IF([NLS_ENABLED], [
	AC_SUBST([GETTEXT_PACKAGE], AC_PACKAGE_TARNAME)
	AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],
		["${GETTEXT_PACKAGE}"], [GETTEXT package name])
])

AC_SUBST([PROJECT_NAME], GL_PROJECT_NAME)

LT_INIT

AC_OUTPUT

dnl  EOF

