#!/bin/sh

#libtoolize
if command -v glibtoolize >/dev/null 2>&1 ; then
  #Darwin based Systems like Mac OS X: libtoolize is called glibtoolize.
  glibtoolize --automake
else
  libtoolize --automake
fi

aclocal
automake -a

autoreconf -fi
