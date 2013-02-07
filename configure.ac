#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([watch_it], [1.0], [mr.wiklander@gmail.com])

# Checks for programs.
AC_PROG_CC

# Checks for header files.
a=1
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h strings.h syslog.h unistd.h],[],[a=0])
if test $a == 0
then
		AC_MSG_WARN([Error, there are missing header files])
		exit
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_MSG_NOTICE([Checking characteristics])
a=1
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([clock_gettime memset modf strcasecmp strchr strerror strstr strtol])

# ARGS
AC_ARG_WITH(conf, [--with-conf 	path to the config file. Default is: /etc/watch_it.conf])
if test "$with_conf" == ""
then
		AC_SUBST(conf,/etc/watch_it.conf)
else 
		AC_SUBST(conf,"$with_conf")
fi


				
AC_OUTPUT(test)
