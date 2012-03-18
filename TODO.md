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

### Ahead ###

   * pagekite.c: libev based single-kite pagekite proxy
   * pagekite.c: multiple-kite pagekite proxy
   * pkmanager.c: reconnect automatically
   * pkmanager.c: choose best front-end automatically
   * pkmanager.c: SOCKS and HTTP proxy support for outgoing connections
   * libpagekite: create simple fork/thread/anonymous-pipe based API for
                  easily adding PageKite support to existing TCP/IP servers.
   * pkproto.c: add ZChunk support
   * pkproto.c: add SSL support

