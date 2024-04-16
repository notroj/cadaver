
dnl CHECK_READLINE checks for presence of readline on the
dnl system.

AC_DEFUN([CHECK_READLINE], [

AC_ARG_ENABLE(readline,
	[  --disable-readline      disable readline support ],
	[use_readline=$enableval],
	[use_readline=yes])  dnl Defaults to ON (if found)

AS_IF([test "$use_readline" = "yes"], [
	AC_CHECK_LIB([curses], [tputs], [LIBS="$LIBS -lcurses"],
		[AC_CHECK_LIB([ncurses], [tputs])])
	AC_CHECK_LIB([readline], [readline])

	AC_SEARCH_LIBS([add_history], [history],
		AC_DEFINE([HAVE_ADD_HISTORY], [1], [Define if you have the add_history function])
	)

	AC_CHECK_HEADERS([history.h readline/history.h readline.h readline/readline.h])

	# Check for rl_completion_matches as in readline 4.2
	AC_CHECK_FUNCS([rl_completion_matches])

	msg_readline="enabled"
], [
	msg_readline="disabled"
])])
