#!/bin/sh
set -ex
rm -rf config.cache autom4te*.cache aclocal.m4
${ACLOCAL:-aclocal} -I macros -I m4
${AUTOHEADER:-autoheader}
${AUTOCONF:-autoconf}
rm -rf autom4te*.cache
rm -rf intl
#gettextize --force --copy --intl | egrep -v ^Copying
