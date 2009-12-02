#!/bin/sh
set -ex
rm -rf config.cache autom4te*.cache aclocal.m4
${ACLOCAL:-aclocal} -I m4 -I m4/neon
${AUTOHEADER:-autoheader}
${AUTOCONF:-autoconf}
rm -rf autom4te*.cache
rm -rf lib/intl
cp -r /usr/share/gettext/intl lib/intl
# Set correct top_builddir for bundled gettext
sed -i '/^top_builddir/s,\.\.,../..,;s,\.\./config.h,../../config.h,g' \
       lib/intl/Makefile.in
