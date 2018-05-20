# PageKite in C #

These are Things TO DO for libpagekite.


## Known Bugs ##

   * pkproto.c: Fix parser to fragment chunks that are too big to fit in the
                available buffer space.
   * SSL certificates are not verified


## Milestones ##

### Ahead / In progress ###

L/M/H: Low/medium/high priority.

   * [H] pkproto.c: add SSL support - finish cert checking code
   * [L] pkproto.c: add ZChunk support
   * [L] pkmanager.c: SOCKS and HTTP proxy support for outgoing connections
   * [M] pagekite.c: An API method for shutting down a live tunnel
   * [M] pagekite.c: An API method for configuring a tunnel w/o restart
   * [-] pkhooks.c & events.py: Add more useful events
      * [H] Generic: Non-blocking gathering of statistics
      * [H] Front-end: authentication of incoming kite requests
      * [M] Front-end: ACLs , quota checks
      * [L] Back-end: traffic introspection
      * [L] Back-end: traffic injection / modification?
   * [M] swarm.py: Implement a front-end relay using pagekite.py configs


### Reached ###

   * pkproto.c: parse pagekite proto, emitting a callback for each chunk
   * pkproto.c: serialize the pagekite protocol to buffer or file descriptor
   * httpkite.c: Implement a very basic back-end (hello world)
   * pagekite.c: libev based single-kite pagekite proxy (blocking)
   * pagekite.c: multiple-kite pagekite proxy (blocking, BE use only)
   * pkmanager.c: reconnect automatically
   * pagekite-jni.c: Bare-bones android wrapper for libpagekite
   * pkproto.c: Use session IDs for replacing old/dead connections.
   * pkmanager.c: added flow control for individual streams
   * pkmanager.c: added flow control for tunnels
   * pkblocker.c: choose best front-end automatically.
   * pkblocker.c: Update DNS records
   * libpagekite: create simple fork/thread/anonymous-pipe based API for
                  easily adding PageKite support to existing TCP/IP servers.
   * Windows compatibility and build rules
   * pkmanager.c: add listeners, for built in web UI and relays
   * pkmanager.c: make update/reconnect schedule tweakable
   * bindings: auto-generate JNI bindings and API documentation
   * bindings: auto-generate Python bindings and API documentation
   * pkhooks.c: Message-based callback API
   * bindings/python/libpagekite/events.py: Higher level API for callbacks
