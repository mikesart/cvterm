#!/bin/bash

PROJNAME=cvterm

BASEDIR=$(dirname $0)
cd $BASEDIR

DIRS=(.. ../../libvterm)

# Create blah.creator file if it doesn't exist
if [[ ! -f ${PROJNAME}.creator ]]; then echo -e "[General]\n" > ${PROJNAME}.creator; fi

# Create our defines file
DEFINES=$(cat <<'END_HEREDOC'
#define __LINUX__ 1
#define _GNU_SOURCE 1
#define HAVE_CURSES_H 1
#define HAVE_CONFIG_H 1
#define CLOG_MAIN 1
END_HEREDOC
)
echo "${DEFINES}" > ${PROJNAME}.config

> ${PROJNAME}.files
for i in ${DIRS[@]}; do
    echo Checking ${i}

    find ${i} -type f -iregex '.*\.\(c\|cxx\|cpp\|h\|lpp\|ypp\|sh\|txt\)$' -or -iname Makefile >> ${PROJNAME}.files
done

find /usr/include -iname curses.h >> ${PROJNAME}.files

fgrep -i --color=never ".h" *.files  | xargs -I{} dirname {} | sort | uniq > ${PROJNAME}.includes

wc -l ${PROJNAME}.files
wc -l ${PROJNAME}.includes
