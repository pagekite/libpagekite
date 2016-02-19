#!/bin/bash
set -e -x
cython --embed -o libtest.c libtest.py
gcc -g -I /usr/include/python2.7 -o libtest libtest.c \
    -lpython2.7 -lpthread -lm -lutil -ldl
rm -f libtest.c
ls -l libtest
