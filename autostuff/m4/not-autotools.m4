dnl  -*- Mode: M4; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-

dnl  **************************************************************************
dnl         _   _       _      ___        _        _              _     
dnl        | \ | |     | |    / _ \      | |      | |            | |    
dnl        |  \| | ___ | |_  / /_\ \_   _| |_ ___ | |_ ___   ___ | |___ 
dnl        | . ` |/ _ \| __| |  _  | | | | __/ _ \| __/ _ \ / _ \| / __|
dnl        | |\  | (_) | |_  | | | | |_| | || (_) | || (_) | (_) | \__ \
dnl        \_| \_/\___/ \__| \_| |_/\__,_|\__\___/ \__\___/ \___/|_|___/
dnl
dnl            A collection of useful m4-ish macros for GNU Autotools
dnl
dnl                                               -- Released under GNU GPL3 --
dnl
dnl                                  https://github.com/madmurphy/not-autotools
dnl  **************************************************************************


dnl  **************************************************************************
dnl  NOTE:  This is only a selection of macros from the **Not Autotools**
dnl         project without documentation. For the entire collection and the
dnl         documentation please refer to the project's website.
dnl  **************************************************************************



dnl  n4_mem([macro-name1[, macro-name2[, ... macro-nameN]]], value)
dnl  **************************************************************************
dnl
dnl  Expands to `value` after this has been stored into one or more macros
dnl
dnl  From: not-autotools/m4/not-m4sugar.m4
dnl
m4_define([n4_mem],
	[m4_if([$#], [0], [],
		[$#], [1], [$1],
		[m4_define([$1], [$2])n4_mem(m4_shift($@))])])


dnl  NA_SANITIZE_VARNAME(string)
dnl  **************************************************************************
dnl
dnl  Replaces `/\W/g,` with `'_'` and `/^\d/` with `_\0`
dnl
dnl  From: not-autotools/m4/not-autotools.m4
dnl
AC_DEFUN([NA_SANITIZE_VARNAME],
	[m4_if(m4_bregexp(m4_normalize([$1]), [[0-9]]), [0], [_])[]m4_translit(m4_normalize([$1]),
		[ !"#$%&\'()*+,-./:;<=>?@[\\]^`{|}~],
		[__________________________________])])


dnl  NC_REQUIRE(macro1[, macro2[, macro3[, ... macroN]]])
dnl  **************************************************************************
dnl
dnl  Variadic version of `AC_REQUIRE()` that can be invoked also from the
dnl  global scope
dnl
dnl  From: not-autotools/m4/not-autotools.m4
dnl
AC_DEFUN([NC_REQUIRE],
	[m4_if([$#], [0],
		[m4_warn([syntax],
			[macro NC_REQUIRE() has been called without arguments])],
		[m4_ifblank([$1],
			[m4_warn([syntax],
				[ignoring empty argument in NC_REQUIRE()])],
			[AC_REQUIRE(m4_normalize([$1]))])[]m4_if([$#], [1], [],
			[m4_newline()NC_REQUIRE(m4_shift($@))])])])


dnl  NA_ESC_APOS(string)
dnl  **************************************************************************
dnl
dnl  Escapes all the occurrences of the apostrophe character in `string`
dnl
dnl  Requires: nothing
dnl  From: not-autotools/m4/not-autotools.m4
dnl
AC_DEFUN([NA_ESC_APOS],
	[m4_bpatsubst([$@], ['], ['\\''])])


dnl  NC_SUBST_NOTMAKE(var[, value])
dnl  **************************************************************************
dnl
dnl  Calls `AC_SUBST(var[, value])` immediately followed by
dnl  `AM_SUBST_NOTMAKE(var)`
dnl
dnl  Requires: nothing
dnl  From: not-autotools/m4/not-autotools.m4
dnl
AC_DEFUN([NC_SUBST_NOTMAKE], [
	AC_SUBST([$1][]m4_if([$#], [0], [], [$#], [1], [], [, [$2]]))
	AM_SUBST_NOTMAKE([$1])
])


dnl  NC_GLOBAL_LITERALS(name1, [val1][, name2, [val2][, ... nameN, [valN]]])
dnl  **************************************************************************
dnl
dnl  For each `nameN`-`valN` pair, creates a new argumentless macro named
dnl  `[GL_]nameN` (where the `GL_` prefix stands for "Global Literal") and a
dnl  new output substitution named `nameN`, both expanding to `valN` when
dnl  invoked
dnl
dnl  Requires: `NA_SANITIZE_VARNAME()` and `NA_ESC_APOS()`
dnl  From: not-autotools/m4/not-autotools.m4
dnl
AC_DEFUN([NC_GLOBAL_LITERALS],
	[m4_if([$#], [0], [], [$#], [1], [],
		[m4_pushdef([_lit_], m4_quote(NA_SANITIZE_VARNAME([$1])))[]m4_define([GL_]m4_quote(_lit_),
			m4_normalize([$2]))[]m4_newline()[]AC_SUBST(_lit_,
			[']NA_ESC_APOS(m4_normalize([$2]))['])[]m4_popdef([_lit_])[]m4_if([$#], [2], [],
			[NC_GLOBAL_LITERALS(m4_shift2($@))])])])


dnl  EOF

