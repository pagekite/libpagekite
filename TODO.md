# PageKite in C #

These are Things TO DO for libpagekite.


## Known Bugs ##

   * pkproto.c: Fix parser to fragment chunks that are too big to fit in the
                available buffer space.



## Milestones ##

### Reached ###

   * pkproto.c: parse pagekite proto, emitting a callback for each chunk
   * pkproto.c: serialize the pagekite protocol to buffer or file descriptor
   * httpkite.c: Implement a very basic back-end (hello world)
   * pagekite.c: libev based single-kite pagekite proxy (blocking)
   * pagekite.c: multiple-kite pagekite proxy (blocking, BE use only)
   * pkmanager.c: reconnect automatically
   * pagekite-jni.c: Bare-bones android wrapper for libpagekite
   * pkproto.c: Use session IDs for replacing old/dead connections.

### Ahead ###

   * pkmanager.c: choose best front-end automatically.
   * pkmanager.c: add flow control
   * libpagekite: create simple fork/thread/anonymous-pipe based API for
                  easily adding PageKite support to existing TCP/IP servers.
   * pkproto.c: add ZChunk support
   * pkproto.c: add SSL support
   * pkmanager.c: SOCKS and HTTP proxy support for outgoing connections
   * pagekite.c: truly non-blocking pagekite proxy, suitable for front-end
   * PageKiteAPI.java: Turn on/off pagekite thread depending on network state.
   * PageKiteAPI.java: Use imprecise timers for opportunistic pings.

### External ###

   * pagekite.py: only very rarely, if ever, pings mobile devices.
   * pagekite.py: block certain requests server side to save traffic.
   * Maybe: pagekite.py, queue incoming requests in case of reconnect?

