dnl  Process this file with autoconf to produce a configure script.

m4_include([autostuff/m4/not-autotools.m4])
m4_include([autostuff/m4/ax_pthread.m4])

AC_PREREQ([2.69])

AC_INIT([GNUnet Worker],
	[0.4.1],
	[madmurphy333@gmail.com],
	[lib]n4_mem([GL_PROJECT_NAME],
		[gnunetworker]),
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

AC_CONFIG_FILES(m4_normalize([
	Makefile
	src/Makefile
	src/]GL_PROJECT_NAME[.pc
	po/Makefile.in
]))

AM_INIT_AUTOMAKE([1.11])

AM_SILENT_RULES([yes])

AC_PROG_CC

AX_PTHREAD

AC_PROG_GREP

AC_CHECK_PROG([HAVE_PKGCONFIG], [pkg-config], [yes], [no])

AS_IF([test "x${HAVE_PKGCONFIG}" != 'xyes'],
	[AC_MSG_ERROR([you need to have pkgconfig installed!])])

PKG_CHECK_MODULES([GNUNET_WORKER], [gnunetutil])

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

NM_AUTO_QUERY_PROGS([find], [doxygen])

AM_GNU_GETTEXT([external])

AM_GNU_GETTEXT_VERSION([0.19])

AM_CONDITIONAL([NLS_ENABLED], [test "x${USE_NLS}" != xno])

AM_COND_IF([NLS_ENABLED], [
	AC_SUBST([GETTEXT_PACKAGE], AC_PACKAGE_TARNAME)
	AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],
		["${GETTEXT_PACKAGE}"], [GETTEXT package name])
])

AM_CONDITIONAL([DIST_DOCS_DIR], [test -d 'docs' -o "x${DOXYGEN}" != 'x'])

AC_SUBST([PROJECT_NAME], GL_PROJECT_NAME)

LT_INIT

AC_OUTPUT

dnl  EOF

