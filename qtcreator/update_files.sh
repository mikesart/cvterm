#!/bin/bash

BASEDIR=$(dirname $0)
cd $BASEDIR
find .. -type f -iregex '.*\.\(c\|cxx\|cpp\|h\|lpp\|ypp\|sh\|txt\)$' -or -iname Makefile > nvterm.files
find ../../libvterm -type f -iregex '.*\.\(c\|cxx\|cpp\|h\|lpp\|ypp\|sh\|txt\)$' -or -iname Makefile >> nvterm.files
