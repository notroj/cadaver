#!/bin/sh -ex
# Script to run update-po and update-gmo before generating
# a release tarball.  Run from .release.sh, *before*
# autogen.sh. 

inmk=Makefile.in.in
tmpmk=`mktemp /tmp/cadaver.XXXXXX`
pot=`mktemp /tmp/cadaver.XXXXXX`
trap 'rm -f $tmpmk $pot' TERM INT 0

cd po

lngs=`cat LINGUAS`
for l in $lngs; do
    CATALOGS="$CATALOGS $l.gmo"
    POFILES="$POFILES $l.po"
done
GMOFILES="$CATALOGS"

sed -e "/^#/d" -e "/^[ 	]*\$/d" -e "s,.*,     ../& \\\\," \
    -e "\$s/\(.*\) \\\\/\1/" < POTFILES.in > $pot

sed -e "
/POTFILES =/r $pot;
s/@SET_MAKE@//g;
s/@PACKAGE@/cadaver/g;
s/@VERSION@/$1/g;
/^.*VPATH.*$/d;
1i\
DOMAIN = cadaver
s/@srcdir@/./g;
s/@top_srcdir@/../g;
s/@CATALOGS@/$CATALOGS/g;
s/@POFILES@/$POFILES/g;
s/@UPDATEPOFILES@/$POFILES/g;
s/@GMOFILES@/$GMOFILES/g;
s/@GMSGFMT@/msgfmt/g;
s/@MSGFMT@/msgfmt/g;
s/@XGETTEXT@/xgettext/g;
s/@MSGMERGE@/msgmerge/g;
s/: Makefile.*/:/g;
s/\$(MAKE) update-gmo/echo Done/g;" $inmk > $tmpmk

make -f $tmpmk cadaver.pot-update ${GMOFILES}
