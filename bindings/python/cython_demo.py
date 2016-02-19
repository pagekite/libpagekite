#!/usr/bin/python
#
# This is a minimal test of the Python libpagekite bindings
# in a script compiled with Cython.
#
# Usage: 
# 1) Build with make
# 2) ./cython_demo <local-port> <protocol> <kitename> <secret>
#
import sys
import time
from libpagekite import *

with PageKite().init_pagekitenet(
        'Python Test',
        1,                # max kites
        50,               # max connections
        PK_WITH_DEFAULTS,
        PK_LOG_NORMAL) as pk:

    pk.add_kite(
        sys.argv[2],      # proto
        sys.argv[3],      # kitename
        0,                # protocol
        sys.argv[4],      # secret
        'localhost',
         int(sys.argv[1])) # local port

    pk.thread_start()

    while True:
        time.sleep(60)
