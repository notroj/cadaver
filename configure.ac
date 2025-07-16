dnl configure script

AC_PREREQ([2.69])
AC_INIT([cadaver],[0.27],,,[https://github.com/notroj/cadaver])

AC_CONFIG_SRCDIR(src/cadaver.c)
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4 neon/macros])

AC_USE_SYSTEM_EXTENSIONS

AC_DEFINE([_GNU_SOURCE], 1, [Define to enable GNU extensions])

AC_PROG_CC
AC_PROG_INSTALL
AC_LANG([C])
AC_PROG_MAKE_SET
AC_PROG_RANLIB

PKG_PROG_PKG_CONFIG

AC_ARG_ENABLE(debugging,
AS_HELP_STRING([--disable-debugging], [disable runtime debugging messages]),,
    enable_debug=yes)

if test "$enable_debug" = "yes"; then
   AC_DEFINE(NE_DEBUGGING, 1, [Define to enable debugging])
fi

CHECK_READLINE()
AM_ICONV

AC_ARG_ENABLE(netrc,
AS_HELP_STRING([--disable-netrc],[enable .netrc support]),,
    enable_netrc=yes)

NEON_WITH_LIBS

m4_ifndef([NE_MINIMUM_VERSION],
        [AC_FATAL([NE_MINIMUM_VERSION macro from neon>=0.34.x required to build.])])

NE_MINIMUM_VERSION(0, 29)

dnl Don't enable zlib or ACL support in neon
NEON_WITHOUT_ZLIB
NEON_WITHOUT_ACL

AC_PROVIDE([NE_SNPRINTF])

NEON_VPATH_BUNDLED(['$(top_srcdir)/neon/src'], [neon/src], [
 NEON_NORMAL_BUILD
 NEON_XML_PARSER
 TARGET_DEPS=neon/src/libneon.a
])

AC_SUBST(TARGET_DEPS)

NE_FORMAT_TIMET

if test "$enable_netrc" = "yes"; then
   LIBOBJS="$LIBOBJS lib/netrc.o"
   AC_DEFINE(ENABLE_NETRC, 1, [Define to enable netrc support])
   AC_MSG_NOTICE(.netrc support is enabled)
else
   AC_MSG_NOTICE(.netrc support is not enabled)
fi

dnl Check for getopt_long
AC_CHECK_FUNC(getopt_long,,
  [LIBOBJS="$LIBOBJS lib/getopt.o lib/getopt1.o"])

dnl Check for strcasecmp
AC_CHECK_FUNC(strcasecmp,,[LIBOBJS="$LIBOBJS lib/strcasecmp.o"])

NEON_REPLACE_SNPRINTF()

AC_FUNC_FNMATCH
if test $ac_cv_func_fnmatch_works = no; then
  LIBOBJS="$LIBOBJS lib/fnmatch.o"
fi

AC_HEADER_DIRENT
AC_STRUCT_DIRENT_D_INO

AC_TYPE_UNSIGNED_LONG_LONG_INT

AC_CHECK_HEADERS(sys/time.h fcntl.h pwd.h stdarg.h vmsdir.h memory.h alloca.h ncurses.h sgtty.h termios.h termio.h locale.h)

AC_CHECK_FUNCS(strdup strerror memcpy strcoll tcsetattr getpass stty fchmod strtoull gettimeofday secure_getenv setlocale)

AC_CACHE_CHECK([whether GLOB_ALTDIRFUNC is available], [cad_cv_glob_altdirfunc],
               [AC_COMPILE_IFELSE(
               [AC_LANG_PROGRAM(
                  [[#include <stddef.h>
#include <glob.h>]],
                  [[glob_t gl = {0}; (void) glob("test", GLOB_ALTDIRFUNC, NULL, &gl);]])],
                  [cad_cv_glob_altdirfunc=yes], [cad_cv_glob_altdirfunc=no])])

if test x$cad_cv_glob_altdirfunc = xno; then
   LIBOBJS="$LIBOBJS lib/glob/glob.o"
   CPPFLAGS="$CPPFLAGS -I\$(top_srcdir)/lib/glob"
fi

dnl Required for AM_GNU_GETTEXT:
PACKAGE=cadaver
VERSION="$PACKAGE_VERSION"
AC_SUBST(VERSION)
AC_SUBST(PACKAGE)
mkdir_p='$(MKDIR_P)'
AC_SUBST(mkdir_p)

AM_GNU_GETTEXT_VERSION([0.22.5])
AM_GNU_GETTEXT([external])
AM_LANGINFO_CODESET

if test "$USE_NLS" = "yes"; then
    nls_msg="Built using native support"
    LIBS="$LIBS $LIBINTL"
else
    nls_msg="Not built"
fi

NEON_WARNINGS

# Pick up config.h etc; and before any other paths.
CPPFLAGS="-I\$(top_builddir) -I\$(top_srcdir)/lib $CPPFLAGS"

AC_CONFIG_FILES([Makefile po/Makefile.in neon/src/Makefile])

AC_OUTPUT

AC_MSG_NOTICE([Configured to build cadaver ${PACKAGE_VERSION}:

  Install prefix:        ${prefix}
  Compiler:              ${CC}
  Neon library:	         ${neon_library_message}
  XML Parser:            ${neon_xml_parser_message}
  SSL library:		 ${ne_SSL_message}
  Internationalization:  ${nls_msg}
  GNU readline support:	 ${msg_readline}
])

test -d src || mkdir src
