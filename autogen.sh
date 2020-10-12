#!/bin/sh
set -ex
rm -rf config.cache autom4te*.cache aclocal.m4
${ACLOCAL:-aclocal} -I m4 -I m4/neon
${AUTOHEADER:-autoheader}
${AUTOCONF:-autoconf}
${LIBTOOLIZE:-libtoolize} --copy --force >/dev/null
rm -rf autom4te*.cache

