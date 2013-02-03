#!/bin/sh

AC_SEARCH_OPTS=""

# This is to make life easier for people who installed pkg-config in /usr/local
# but have autoconf, make, etc in /usr/. AKA most mac users
if [ -d "/usr/local/share/aclocal" ]
then
    AC_SEARCH_OPTS="-I /usr/local/share/aclocal"
fi

aclocal $AC_SEARCH_OPTS && \
autoconf && \
autoheader && \
automake --add-missing && \
./configure && \
lua bin2c.lua lua_diff_match_patch_str < src/lua/diff_match_patch.lua > src/dmp_lua_str.h && \
make
