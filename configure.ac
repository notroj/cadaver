dnl configure script

AC_PREREQ([2.71])
AC_INIT([cadaver],[0.23.3],[cadaver@webdav.org])

AC_CONFIG_SRCDIR(src/cadaver.c)
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4 m4/neon])

AC_USE_SYSTEM_EXTENSIONS

AC_DEFINE([_GNU_SOURCE], 1, [Define to enable GNU extensions])

NEON_WITH_LIBS

AC_PROG_CC
AC_PROG_INSTALL
AC_LANG([C])
AC_PROG_MAKE_SET
AC_PROG_RANLIB

AC_HEADER_STDC

AC_ARG_ENABLE(debugging,
AS_HELP_STRING([--disable-debugging], [disable runtime debugging messages]),,
    enable_debug=yes)

if test "$enable_debug" = "yes"; then
   AC_DEFINE(NE_DEBUGGING, 1, [Define to enable debugging])
fi

CHECK_READLINE()

AC_ARG_ENABLE(netrc,
AS_HELP_STRING([--disable-netrc],[enable .netrc support]),,
    enable_netrc=yes)

NE_REQUIRE_VERSIONS([0], [27 28 29 30 31 32])

dnl Don't enable zlib or ACL support in neon
NEON_WITHOUT_ZLIB
NEON_WITHOUT_ACL

AC_PROVIDE([NE_SNPRINTF])

NEON_LIBRARY

NE_FORMAT_TIMET

if test "$enable_netrc" = "yes"; then
   AC_LIBOBJ(lib/netrc)
   AC_DEFINE(ENABLE_NETRC, 1, [Define to enable netrc support])
   AC_MSG_NOTICE(.netrc support is enabled)
else
   AC_MSG_NOTICE(.netrc support is not enabled)
fi

dnl Check for getopt_long
AC_CHECK_FUNC(getopt_long,,[AC_LIBOBJ(lib/getopt)
AC_LIBOBJ(lib/getopt1)])

dnl Check for strcasecmp
AC_CHECK_FUNC(strcasecmp,,[AC_LIBOBJ(lib/strcasecmp)])

AC_HEADER_TIME

jm_PREREQ_TEMPNAME

NEON_REPLACE_SNPRINTF()

AC_FUNC_FNMATCH
if test $ac_cv_func_fnmatch_works = no; then
  AC_LIBOBJ(lib/fnmatch)
fi

AC_HEADER_DIRENT

AC_TYPE_SIGNAL

AC_TYPE_UNSIGNED_LONG_LONG_INT

AC_CHECK_FUNCS(strdup strerror memcpy strcoll tcsetattr getpass stty fchmod strtoull)
AC_CHECK_HEADERS(sys/time.h pwd.h stdarg.h vmsdir.h memory.h alloca.h ncurses.h sgtty.h termios.h termio.h locale.h)

jm_FUNC_STRFTIME()

AM_GNU_GETTEXT([external])

if test "$USE_NLS" = "yes"; then
    nls_msg="Built using native support"
    LIBS="$LIBS $LIBINTL"
else
    nls_msg="Not built"
fi

NEON_WARNINGS

# Pick up config.h etc; and before any other paths.
CPPFLAGS="-I\$(top_builddir) -I\$(top_srcdir)/lib $CPPFLAGS"

AC_CONFIG_FILES([Makefile lib/neon/Makefile \
                 po/Makefile.in])

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
