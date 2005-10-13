dnl configure script

AC_PREREQ(2.53)
AC_INIT(cadaver, 0.22.0, cadaver@webdav.org)

AC_CONFIG_SRCDIR(src/cadaver.c)
AC_CONFIG_HEADER(config.h)

PACKAGE=cadaver

AC_DEFINE([_GNU_SOURCE], 1, [Define to enable GNU extensions])
AC_DEFINE([USE_DAV_LOCKS], 1, [Build support for WebDAV locking through neon])

NEON_WITH_LIBS

AC_ISC_POSIX
AC_PROG_CC
AC_PROG_INSTALL
AC_LANG_C
AC_SET_MAKE
AC_PROG_RANLIB

AC_HEADER_STDC

AC_ARG_ENABLE(debugging,
AC_HELP_STRING([--disable-debugging],[disable runtime debugging messages]),,
enable_debug=yes)

if test "$enable_debug" = "yes"; then
   AC_DEFINE(NE_DEBUGGING, 1, [Define to enable debugging])
fi

CHECK_READLINE()

AC_ARG_ENABLE(netrc,
AC_HELP_STRING([--disable-netrc], [enable .netrc support]),,
enable_netrc=yes)

NE_REQUIRE_VERSIONS([0], [24 25])

dnl Don't enable zlib or ACL support in neon
NEON_WITHOUT_ZLIB
NEON_WITHOUT_ACL

AC_PROVIDE([NE_SNPRINTF])

dnl Build neon. We support a VPATH build.
NEON_VPATH_BUNDLED(['$(top_srcdir)/lib/neon'], [lib/neon], [
	# We are using a bundled build, do the necessary stuff...
	# we have a bundled expat, and we don't need zlib support
        NEON_XML_PARSER([\$(top_srcdir)/lib/expat], [\$(top_builddir)/lib/expat])
	NEON_NORMAL_BUILD
], [
	# Not using a bundled build... do stuff.
	AC_C_CONST
	AC_CHECK_HEADERS(stdlib.h string.h strings.h unistd.h)
])

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

AC_CHECK_FUNCS(strdup strerror memcpy strcoll tcsetattr getpass stty)
AC_CHECK_HEADERS(sys/time.h pwd.h stdarg.h vmsdir.h memory.h alloca.h ncurses.h sgtty.h termios.h termio.h)

jm_FUNC_STRFTIME()

dnl joe: disabling this because it causes portability problems (particularly
dnl to Mac OS X, it seems), and there are not translations yet.  Will turn
dnl it on again when there are translations.
dnl AM_GNU_GETTEXT([no-libtool])

dnl if test "$USE_NLS" = "yes"; then
dnl	# joe: how to do this with less mucky quotes, please?
dnl	CFLAGS="$CFLAGS '-DLOCALEDIR=\"$(localedir)\" -DGNULOCALEDIR=\"$(gnulocaledir)\" -DLOCALE_ALIAS_PATH=\"$(aliaspath)\"'"
dnl	NLSINSTALL="install-nls"
dnl	if test "$USE_INCLUDED_LIBINTL" = "yes"; then
dnl		# Using included NLS stuff
dnl		CFLAGS="$CFLAGS -I\$(top_srcdir)/intl"
dnl		nls_msg="Built using included libintl"
dnl		LIBS="-Lintl -lintl $LIBS"
dnl	else
dnl		nls_msg="Built using native support"
dnl		# ??? CFLAGS? LIBS?
dnl	fi
dnl	SUBDIRS="$SUBDIRS intl po"
dnl else

nls_msg="Not built"
DATADIRNAME=share

NEON_WARNINGS

# Pick up config.h
CPPFLAGS="$CPPFLAGS -I\$(top_builddir) -I\$(top_srcdir)/lib"

AC_SUBST(DATADIRNAME)
AC_SUBST(NLSINSTALL)

AC_SUBST(SDEFS)
AC_SUBST(PACKAGE)
AC_SUBST(LIBS)
AC_SUBST(CFLAGS)
AC_SUBST(SUBDIRS)

AC_CONFIG_FILES([Makefile lib/neon/Makefile])

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

