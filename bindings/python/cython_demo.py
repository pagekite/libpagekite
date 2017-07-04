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
        sys.argv[2],       # protocol
        sys.argv[3],       # kitename
        0,                 # port, 0 = use default
        sys.argv[4],       # secret
        'localhost',
         int(sys.argv[1])) # local port

    # Note: We should also pk.add_frontend with the kite name, to
    #       avoid DNS cache/ttl issues on startup/restart.

    pk.thread_start()

    while True:
        event_id = pk.await_event(1)
        pk.event_respond(event_id, PK_EV_RESPOND_DEFAULT)

        if (event_id != PK_EV_NONE):
            print("Got event: %x" % event_id)

        if ((event_id & PK_EV_TYPE_MASK) == PK_EV_SHUTDOWN):
            break
