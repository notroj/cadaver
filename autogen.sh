#!/bin/sh
set -ex
rm -rf config.cache autom4te*.cache aclocal.m4
${ACLOCAL:-aclocal} -I m4 -I m4/neon
${AUTOHEADER:-autoheader}
${AUTOCONF:-autoconf}
rm -rf autom4te*.cache
rm -rf intl
#gettextize --force --copy --intl | egrep -v ^Copying
