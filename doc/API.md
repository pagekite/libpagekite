# The libpagekite public API

This is the public API of libpagekite. These methods should remain
stable over the long term, even if the internals of the library get
shuffled around and changed.

Contents:

1. Overview
2. Hello-world example
3. Library Reference
4. Platform specifics


## 1. Overview

Libpagekite provides a simple interface for configuring and running a
PageKite "back-end connector", which takes care of registering one or
more kites, selecting front-end relays and keeping the connection
active.


### 1.1. Lifecycle overview

1. The caller will create a manager object (`pagekite_mgr`) in one of
   two modes, with PageKite.net service or without:
   `pagekite_init_pagekitenet` or `pagekite_init`
2. This manager object is configured with front-end relays and kites:
   `pagekite_add_frontend` and `pagekite_add_kite`
3. Optionally, various settings can be adjusted using `pagekite_set*`,
   `pagekite_enable*`, etc.
4. A thread is spawned which runs the libpagekite event loop:
   `pagekite_start`
5. The event loop spawns a 2nd background thread for housekeeping tasks
6. The caller can query the status of the libpagekite manager thread:
   `pagekite_get_status` or `pagekite_get_log`
7. The caller can shut down the manager thread: `pagekite_stop`
8. The caller calls `pagekite_free` to release any allocated resources

All methods, except the initialization functions, take a `pagekite_mgr`
object as thei first argument.

All API methods either return an integer or a pointer. In case of error,
negative values or `NULL` are returned. Error states can be explained
using `pagekite_perror`.


### 1.2. Current known limitations

These are some of the limitations of the current implementation.
Hopeefully over time most (or even all) of these will be fixed:

1. The front-ends are currently configured using a DNS lookup, which
    means initialization may fail if there is no network connectivity.
2. The manager configuration is almost completely static - in order to add
   or remove kites or update the list of available frontends, the
   manager object must be stopped, destroyed and recreated from scratch.
3. Thread safety: There are some global variables used internally by
   `libpagekite`, so although instanciating the library multiple times and
   running multiple workers within the same app should mostly work , they
   currently will share some settings, in particular to do with error
   handling and logging.


## 2. Hello-world example

`contrib/backends/hello.c`:

    #include <assert.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include <pagekite.h>

    int main(int argc, const char** argv) {
        pagekite_mgr pkm;

        if (argc < 3) {
            fprintf(stderr, "Usage: hello yourkite.pagekite.me password\n");
            return 1;
        }

        pkm = pagekite_init_pagekitenet(
            "Hello World App",   /* What app is this?                       */
            1,                   /* Max number of kites we plan to fly      */
            25,                  /* Max number of simultaneous client conns */
            PK_WITH_DEFAULTS,    /* Use defaults (SSL, IPv4 and IPv6)       */
            PK_LOG_NORMAL);      /* Log verbosity level or bitmask          */

        if (pkm == NULL) {
            pagekite_perror(pkm, argv[0]);
            return 2;
        }

        /* Add a kite for the HTTP server on localhost:80 */
        if (0 > pagekite_add_kite(pkm,
                                  "http",          /* Kite protocol         */
                                  argv[1],         /* Kite name             */
                                  0,               /* Public port, 0=any    */
                                  argv[2],         /* Kite secret           */
                                  "localhost",     /* Origin server host    */
                                  80))             /* Origin server port    */
        {
            pagekite_perror(pkm, argv[0]);
            return 3;
        }

        /* Start the worker thread, wait for it to exit. */
        assert(-1 < pagekite_start(pkm));
        assert(-1 < pagekite_wait(pkm));

        /* Not reached: Nothing actually shuts us down in this app. */
        assert(-1 < pagekite_free(pkm));

        return 0;
    }


## 3. Library reference

**This section was last updated on December 8, 2014.**


### 3.1. Methods

### 3.1.1. pagekite_init(...)

This function is called to create a PageKite manager object. It will
allocate a fixed pool of memory with enough space for the requested
number of kites, front-ends and connections.

Definition:

    pagekite_mgr pagekite_init(
      const char* app_id,
      int max_kites,
      int max_frontends,
      int max_conns,
      const char* dyndns_url,
      int flags,
      int verbosity);

Arguments:

* app_id: A human-readable string identifying your app.
* max_kites: The maximum number of kites you plan to fly.
* max_frontends: The maximum number of frontends you want to consider.
* max_conns: The maximum number of simultaneous client connections.
* dyndns_url: NULL or a dymanic-DNS update URL format string.
* flags: A bitmask of `PK_WITH_*` or `PK_WITHOUT_*` constants.
* verbosity: An integer (0, 1, 2, ...) or bitmask of `PK_LOG_*` constants.

Returns:

* Success: A pagekite_mgr object (a pointer)
* Error: NULL

Side-effects:

* Mallocs a chunk of RAM
* Will perform DNS lookups in order to configure front-ends, if the
  `PK_WITH_SERVICE_FRONTENDS` flag is set.
* Creates an OpenSSL context
* Calls WSAStartup (Windows only)


### 3.1.2. pagekite_init_pagekitenet(...)

This function is called to create a PageKite manager object,
preconfigure for use with the PageKite.net public service.

See also `pagekite_init` for details.

Definition:

    pagekite_mgr pagekite_init_pagekitenet(
      const char* app_id,
      int max_kites,
      int max_conns,
      int flags,
      int verbosity);

Arguments:

* app_id: A human-readable string identifying your app.
* max_kites: The maximum number of kites you plan to fly.
* max_conns: The maximum number of simultaneous client connections.
* flags: A bitmask of `PK_WITH_*` or `PK_WITHOUT_*` constants.
* verbosity: An integer (0, 1, 2, ...) or bitmask of `PK_LOG_*` constants.

Returns:

* Success: A pagekite_mgr object (a pointer)
* Error: NULL

Side-effects:

* See `libpagekite_init`


### 3.1.3. pagekite_add_kite(...)

This method configures the manager for flying a kite. This method can be
invoked as many times as the `max_kites` value which was given to the
`pagekite_init` function.

Definition:

    int pagekite_add_kite(
      pagekite_mgr pkm,
      const char* proto,
      const char* kitename,
      int pport,
      const char* secret,
      const char* backend,
      int lport);

Arguments:

* pkm: The pagekite_mgr object.
* proto: A protocol, usually `"http"`, `"https"` or `"raw"`.
* kitename: The DNS name of your kite, e.g. `username.pagekite.me`.
* pport: The public port to listen on, or 0 to accept front-end defaults.
* secret: The shared secret (password) used to authenticate.
* backend: The hostname of the origin server, usually `"localhost"`.
* lport: The port number of the origin server, e.g. 80 for HTTP.

Returns:

* Success: 0
* Error: -1

Side-effects:

* None. 

Notes:

* It depends on the front-end relay which protocols and public ports are
  actually available.
* Generally it is a good idea to leave the public port number undefined
* If the front-end relay offers MITM wild-card SSL, you need to request
  the `"http"` protocol, not `"https"`, in order to make use of it.
* Requesting `"https"` will activate end-to-end SSL (local SSL
  termination).
* In a static (non-dynamic) configuration, you will probably also want
  to invoke `pagekite_add_frontend` using the kite DNS name and public
  port as a front-end relay.


### 3.1.4. pagekite_add_service_frontends(...)

This configures the manager object to use the PageKite.net public
service front-end relays. This is usually invoked internally as part
of `pagekite_init` or `pagekite_init_pagekitenet`, but is exposed
here for apps which want more granular control over the setup process
(see `contrib/pagekitec.c` for an example).

Definition:

    int pagekite_add_service_frontends(pagekite_mgr pkm, int flags);

Arguments:

* pkm: The pagekite_mgr object.
* flags: A bitmask of `PK_WITH_*` and `PK_WITHOUT_*` constants.

Returns:

* Success: 0
* Error: -1

Side-effects:

* Performs DNS lookups in order to resolve PageKite.net front-end relay
  names to one or more IP addresses.

Notes:

* This will fail miserably if you don't have a network connection.


### 3.1.4. pagekite_add_frontend(...)

This configures the manager object to use a given front-end relay.

This function can be called at most as often as specified by the
`max_frontends` argument to `pagekite_init`. In practice, it may run out
of slots much sooner, if the given DNS name resolves to multiple IP
addresses.

Definition:

    int pagekite_add_frontend(pagekite_mgr, char* domain, int port);

Arguments:

* pkm: The pagekite_mgr object.
* domain: The DNS name of the front-end relay(s)
* port: The port to make tunnel connections to (usually 443).

Returns:

* Success: 0
* Error: -1

Side-effects:

* Performs DNS lookups in order to resolve the front-end relay names to
  one or more IP addresses.

Notes:

* This will fail miserably if you don't have a network connection,
  unless it is given IP addresses instead of DNS names.



### 3.1.5. Other methods:

The following methods will be documented better Real Soon Now.

    int pagekite_set_log_mask(pagekite_mgr, int);
    int pagekite_enable_watchdog(pagekite_mgr, int enable);
    int pagekite_enable_fake_ping(pagekite_mgr pkm, int enable);
    int pagekite_set_bail_on_errors(pagekite_mgr pkm, int errors);
    int pagekite_set_conn_eviction_idle_s(pagekite_mgr pkm, int seconds);
    int pagekite_want_spare_frontends(pagekite_mgr, int spares);
    int pagekite_tick(pagekite_mgr);
    int pagekite_poll(pagekite_mgr, int timeout);
    int pagekite_start(pagekite_mgr);
    int pagekite_wait(pagekite_mgr);
    int pagekite_stop(pagekite_mgr);
    int pagekite_get_status(pagekite_mgr);
    char* pagekite_get_log(pagekite_mgr);
    int pagekite_free(pagekite_mgr);
    void pagekite_perror(pagekite_mgr, const char*);


### 3.2. Macros, Constants, Types

The library version:

    PK_VERSION

Flags for `pagekite_init` et al:

    PK_WITH_DEFAULTS             - Use default settings
    PK_WITHOUT_DEFAULTS          - Disable default settings
    PK_WITH_SSL                  - Enable SSL encrypted tunnels
    PK_WITH_IPV4                 - Enable IPv4
    PK_WITH_IPV6                 - Enable IPv6
    PK_WITH_SERVICE_FRONTENDS    - Use PageKite.net service frontends
    PK_WITHOUT_SERVICE_FRONTENDS - Explicitly disable service frontends

Logging constants:

    PK_LOG_TUNNEL_DATA           - Debug tunnel data
    PK_LOG_TUNNEL_HEADERS        - Debug tunnel frame headers
    PK_LOG_TUNNEL_CONNS          - Debug tunnel connection attempts
    PK_LOG_BE_DATA               - Debug origin server data
    PK_LOG_BE_HEADERS            - Currently unused
    PK_LOG_BE_CONNS              - Debug origin server connections
    PK_LOG_MANAGER_ERROR         - Log manager errors
    PK_LOG_MANAGER_INFO          - Log manager information
    PK_LOG_MANAGER_DEBUG         - Log manager debugging details

    PK_LOG_TRACE                 - Trace individual function calls
    PK_LOG_ERROR                 - Log uncategorized errors

Aggregate logging constants, used with `pagekite_set_log_mask`:

    PK_LOG_ERRORS                - Match all error info
    PK_LOG_MANAGER               - Match all manager info
    PK_LOG_CONNS                 - Match all connection info
    PK_LOG_NORMAL                - Default log level
    PK_LOG_DEBUG                 - Default debugging log level
    PK_LOG_ALL                   - Log everything

Pagekite.net service related constants:

    PAGEKITE_NET_DDNS            - Dynamic DNS update URL format
    PAGEKITE_NET_V4FRONTENDS     - IPv4 front-ends: hostname, port
    PAGEKITE_NET_V6FRONTENDS     - IPv6 front-ends: hostname, port
    PAGEKITE_NET_CLIENT_MAX      - Default max number of clients
    PAGEKITE_NET_LPORT_MAX       - Currently unused
    PAGEKITE_NET_FE_MAX          - Default max number of front-end relays

The PageKite manager object:

    typedef pagekite_mgr         - An opaque pointer type


## 4. Platform specifics

### 4.1. Linux

None at the moment.

### 4.2. Android

TBD.

### 4.3. Windows

TBD.

