# PageKite API reference manual

   * Initialization
      * [`init                                        `](#nt)
      * [`initPagekitenet                             `](#ntPgktnt)
      * [`initWhitelabel                              `](#ntWhtlbl)
      * [`addKite                                     `](#ddKt)
      * [`addServiceFrontends                         `](#ddSrvcFrntnds)
      * [`addWhitelabelFrontends                      `](#ddWhtlblFrntnds)
      * [`lookupAndAddFrontend                        `](#lkpAndAddFrntnd)
      * [`addFrontend                                 `](#ddFrntnd)
      * [`setLogMask                                  `](#stLgMsk)
      * [`setLogDestination                           `](#stLgDstntn)
      * [`setHousekeepingMinInterval                  `](#stHskpngMnIntrvl)
      * [`setHousekeepingMaxInterval                  `](#stHskpngMxIntrvl)
      * [`setRejectionUrl                             `](#stRjctnUrl)
      * [`enableHttpForwardingHeaders                 `](#nblHttpFrwrdngHdrs)
      * [`enableFakePing                              `](#nblFkPng)
      * [`enableWatchdog                              `](#nblWtchdg)
      * [`enableTickTimer                             `](#nblTckTmr)
      * [`setConnEvictionIdleS                        `](#stCnnEvctnIdlS)
      * [`setOpensslCiphers                           `](#stOpnsslCphrs)
      * [`wantSpareFrontends                          `](#wntSprFrntnds)
   * Lifecycle
      * [`threadStart                                 `](#thrdStrt)
      * [`threadWait                                  `](#thrdWt)
      * [`threadStop                                  `](#thrdStp)
      * [`free                                        `](#fr)
      * [`setEventMask                                `](#stEvntMsk)
      * [`awaitEvent                                  `](#wtEvnt)
      * [`getEventInt                                 `](#gtEvntInt)
      * [`getEventStr                                 `](#gtEvntStr)
      * [`eventRespond                                `](#vntRspnd)
      * [`eventRespondWithData                        `](#vntRspndWthDt)
      * [`getStatus                                   `](#gtStts)
      * [`getLog                                      `](#gtLg)
      * [`dumpStateToLog                              `](#dmpSttTLg)
      * [`poll                                        `](#pll)
      * [`tick                                        `](#tck)
      * [`setBailOnErrors                             `](#stBlOnErrrs)
      * [`perror                                      `](#prrr)
   * [Constants](#constants)

## Functions

### Initialization

<a                                                           name="nt"><hr></a>

#### `boolean init(...)`

Initialize the PageKite manager.

This allocates a static amount of RAM for PageKite (not counting
buffers managed by OpenSSL). Since libpagekite's resource usage
is allocated up-front, you need to specify maximum numbers of
kites, front-end relays and in-flight connections you want to
keep track of at any one time.

The `flags` variable should be used with the constants `PK_WITH_*`
and `PK_AS_*`, bitwise OR'ed together to tune the behaviour of
libpagekite. To use the recommended defaults, simply specify `PK_WITH_DEFAULTS`.
Requesting defaults is a forward compatible choice and will continue
to use recommended settings even as new features are added to
the library.

The `verbosity` argument controls the internal logging. A small
integer (-1, 0, 1, 2) can be used to choose from a predefined
level, for more fine-grained control use the `PK_LOG_` constants
bitwise OR'ed together to enable logging of individual subsystems.

This method can only be called before starting the master thread.

**Arguments**:

   * `String app_id`: Short ID, reported in instrumentation
   * `int max_kites`: Max number of kite names allowed
   * `int max_frontends`: Max number of front-end relays recognized
   * `int max_conns`: Max number of in-flight connections
   * `String dyndns_url`: Dynamic DNS update URL (format string)
   * `int flags`: Flags, which features to enable
   * `int verbosity`: Verbosity or log mask

**Returns**: True if successful, False otherwise.


<a                                                     name="ntPgktnt"><hr></a>

#### `boolean initPagekitenet(...)`

Initialize the PageKite manager, configured for use with the pagekite.net
public service.

See the basic init docs for further details.

If `flags` do not include `PK_WITHOUT_SERVICE_FRONTENDS`, service
frontends will be chosen automatically using the given domain
name.

This method can only be called before starting the master thread.

**Arguments**:

   * `String app_id`: Short ID, reported in instrumentation
   * `int max_kites`: Max number of kite names allowed
   * `int max_conns`: Max number of in-flight connections
   * `int flags`: Flags, which features to enable
   * `int verbosity`: Verbosity or log mask

**Returns**: True if successful, False otherwise.


<a                                                     name="ntWhtlbl"><hr></a>

#### `boolean initWhitelabel(...)`

Initialize the PageKite manager, configured for use with a pagekite.net
white-label domain.

See the basic init docs for further details.

If `flags` do not include `PK_WITHOUT_SERVICE_FRONTENDS`, white-label
frontends will be chosen automatically using the given domain
name.

This method can only be called before starting the master thread.

**Arguments**:

   * `String app_id`: Short ID, reported in instrumentation
   * `int max_kites`: Max number of kite names allowed
   * `int max_conns`: Max number of in-flight connections
   * `int flags`: Flags, which features to enable
   * `int verbosity`: Verbosity or log mask
   * `String whitelabel_tld`: Top level domain of white-label kites

**Returns**: True if successful, False otherwise.


<a                                                         name="ddKt"><hr></a>

#### `int addKite(...)`

Configure a "kite", mapping a public domain to a local service.

Multiple kites can be configured for the same domain name, by
calling this function multiple times, as long as the public port
or protocol differ.

This method can only be called before starting the master thread.

Note: When used with the "raw" protocol, the public port cannot
be 0.

**Arguments**:

   * `String proto`: Protocol
   * `String kitename`: Kite DNS name
   * `int pport`: Public port, 0 for default/any
   * `String secret`: Kite secret for authentication
   * `String backend`: Hostname of the origin server
   * `int lport`: Port of the origin server

**Returns**: 0 on success, -1 on failure.


<a                                                name="ddSrvcFrntnds"><hr></a>

#### `int addServiceFrontends(...)`

Configure libpagekite to use the Pagekite.net pool of public front-end
relay servers.

This method can only be called before starting the master thread.

**Arguments**:

   * `int flags`: Flags to enable IPv4, IPv6 and/or dynamic frontends

**Returns**: The number of relay IPs configured, or -1 on failure.


<a                                              name="ddWhtlblFrntnds"><hr></a>

#### `int addWhitelabelFrontends(...)`

Configure libpagekite to use the relays associated with a Pagekite.net
white-label domain.

This method can only be called before starting the master thread.

**Arguments**:

   * `int flags`: Flags to enable IPv4, IPv6 and/or dynamic frontends
   * `String whitelabel_tld`: Top level domain of white-label kites

**Returns**: The number of relay IPs configured, or -1 on failure.


<a                                              name="lkpAndAddFrntnd"><hr></a>

#### `int lookupAndAddFrontend(...)`

Configure libpagekite front-end relays.

All available IP addresses referenced by the specified DNS name
will be configured as potential relays, and the app will choose
between them based on performance metrics.

If `update_from_dns` is nonzero, DNS will be rechecked periodically
for new relay IPs. Enabling such updates is generally preferred
over a completely static configuration, so long-running instances
can keep up with changes to the relay infrastructure.

This method can only be called before starting the master thread.

**Arguments**:

   * `String domain`: DNS name of the frontend (or pool of frontends)
   * `int port`: Port to connect to
   * `int update_from_dns`: Set nonzero to re-lookup periodically from DNS

**Returns**: The number of relay IPs configured, or -1 on failure.


<a                                                     name="ddFrntnd"><hr></a>

#### `int addFrontend(...)`

Configure libpagekite front-end relays.

All available IP addresses referenced by the specified DNS name
will be configured as potential relays, and the app will choose
between them based on performance metrics.

This method is static - DNS is only checked on startup. This is
not optimal and this method is only provided for backwards compatibility.
New apps should enable periodic DNS updates.

This method can only be called before starting the master thread.

**Arguments**:

   * `String domain`: DNS name of the frontend (or pool of frontends)
   * `int port`: Port to connect to

**Returns**: The number of relay IPs configured, or -1 on failure.


<a                                                      name="stLgMsk"><hr></a>

#### `int setLogMask(...)`

Configure the log verbosity using a bitmask.

See the `PK_LOG_*` constants for options.

This function can be called at any time.

**Arguments**:

   * `int mask`: A bitmask

**Returns**: Always returns 0.


<a                                                   name="stLgDstntn"><hr></a>

#### `int setLogDestination(...)`

Configure log destination.

This function can be called at any time. The argument should be
either a file descriptor (integer >= 0) or one of the constants
PK_LOG_DEST_SYSLOG or PK_LOG_DEST_NONE.

**Arguments**:

   * `int log_destination`: Log destination

**Returns**: Always returns 0.


<a                                             name="stHskpngMnIntrvl"><hr></a>

#### `int setHousekeepingMinInterval(...)`

Configure the minimum interval for internal housekeeping.

Internal housekeeping includes attempting to re-establish a connection
to the required relays, occasionally refreshing information from
DNS and pinging tunnels to detect whether they have gone silently
dead (due to NAT timeouts, for example).

Setting this interval too low may reduce battery life or increase
load on shared infrastructure, as a result there is a hard-coded
minimum value which this function cannot override. Setting this
interval too high may prevent libpagekite from detecting network
outages and recovering in a timely fashion.

The app will by default choose an interval between the minimum
and maximum as appropriate. Most apps will not need to change
this setting.

This function can be called at any time.

**Arguments**:

   * `int interval`: Interval, in seconds

**Returns**: The new minimum housekeeping interval.


<a                                             name="stHskpngMxIntrvl"><hr></a>

#### `int setHousekeepingMaxInterval(...)`

Configure the maximum interval for internal housekeeping.

See the documentation for the minimum housekeeping interval for
details and performance concerns.

This function can be called at any time.

**Arguments**:

   * `int interval`: Interval, in seconds

**Returns**: The new maximum housekeeping interval.


<a                                                   name="stRjctnUrl"><hr></a>

#### `int setRejectionUrl(...)`

Configure the rejection URL.

See the destination URL included in the IFRAME for incoming requests
that cannot be handled (e.g. because the origin web server is
down or things are misconfigured).

This function can be called at any time.

**Arguments**:

   * `String url`: The new URL

**Returns**: 0


<a                                           name="nblHttpFrwrdngHdrs"><hr></a>

#### `int enableHttpForwardingHeaders(...)`

Enable or disable HTTP forwarding headers.

When enabled, libpagekite will rewrite incoming HTTP headers to
add information about the remote IP address and remote protocol.

The `X-Forwarded-For` header will contain the IP address of the
remote HTTP client, as reported by the front-end relay (probably
in IPv6 notation).

The `X-Forwarded-Proto` header will report whether the connection
to the relay was HTTP or HTTPS. This can be detected and used
to redirect or reject plain-text connections.

**Limitations**: Headers are only added to the first request of
a persistent HTTP/1.1 session. The data reported comes from the
front-end relay and a malicious relay could provide false data.

This function can be called at any time.

**Arguments**:

   * `int enable`: 0 disables, any other value enables

**Returns**: Always returns 0.


<a                                                     name="nblFkPng"><hr></a>

#### `int enableFakePing(...)`

Enable or disable fake pings.

This is a debugging/testing option, which effectively randomizes
which front-end relay is used and increases the frequency of migrations.

This function can be called at any time.

**Arguments**:

   * `int enable`: 0 disables, any other value enables

**Returns**: Always returns 0.


<a                                                    name="nblWtchdg"><hr></a>

#### `int enableWatchdog(...)`

Enable or disable watchdog.

The watchdog is a thread which periodically checks if the main
pagekite thread has locked up. If it thinks the app has locked
up, it will cause a segmentation fault, which in turn will create
a core dump for debugging (assuming ulimits allow).

This method can only be called before starting the master thread.

**Arguments**:

   * `int enable`: 0 disables, any other value enables

**Returns**: Always returns 0.


<a                                                    name="nblTckTmr"><hr></a>

#### `int enableTickTimer(...)`

Enable or disable tick event timer.

This method can be used to toggle the tick event timer on or off
(it is on by default). Apps may want to disable the timer for
power saving reasons.

If the timer is disabled, the tick method should be called instead
when possible, to ensure that housekeeping takes place.

This method may be called at any time, but may hang as it waits
for the main event-loop lock.

**Arguments**:

   * `int enable`: 0 disables, any other value enables

**Returns**: Always returns 0.


<a                                               name="stCnnEvctnIdlS"><hr></a>

#### `int setConnEvictionIdleS(...)`

Configure eviction of idle connections.

As libpagekite works with a fixed pool of RAM, it may be unable
to allocate buffers for new incoming connections. When this happens,
the oldest connection which has been idle for more than `seconds`
seconds may be evicted.

Set `seconds = 0` to disable eviction and instead reject incoming
connections when overloaded. Eviction is disabled by default.

This function can be called at any time.

**Arguments**:

   * `int seconds`: Minimum idle time to qualify for eviction

**Returns**: Always returns 0.


<a                                                name="stOpnsslCphrs"><hr></a>

#### `int setOpensslCiphers(...)`

Choose which ciphers to use in TLS

See the SSL_set_cipher_list(3) and ciphers(1) man pages for details.

This function can be called at any time.

**Arguments**:

   * `String ciphers`: The string passed to OpenSSL

**Returns**: Always returns 0.


<a                                                name="wntSprFrntnds"><hr></a>

#### `int wantSpareFrontends(...)`

Connect to multiple front-end relays.

If non-zero, this setting will configure how many spare relays
to connect to at any given time. This may increase availability
or performance in some special cases, but increases the load on
shared relay infrastructure and should be avoided if possible.

This function can be called at any time.

**Arguments**:

   * `int spares`: Number of spare front-ends to connect to

**Returns**: Always returns 0.

### Lifecycle

<a                                                     name="thrdStrt"><hr></a>

#### `int threadStart(...)`

Start the main thread: run pagekite!

This function should only be called once (per session).

**Arguments**:



**Returns**: The return value of `pthread_create()`


<a                                                       name="thrdWt"><hr></a>

#### `int threadWait(...)`

Wait for the main pagekite thread to finish.

This function should only be called once (per session). Running
this function will implicitly respond to all API events with PK_EV_RESPOND_DEFAULT.

**Arguments**:



**Returns**: The return value of `pthread_join()`


<a                                                      name="thrdStp"><hr></a>

#### `int threadStop(...)`

Stop the main pagekite thread.

This function should only be called once (per session).

**Arguments**:



**Returns**: The return value of `pthread_join()`


<a                                                           name="fr"><hr></a>

#### `int free(...)`

Free the internal libpagekite buffers.

Call this to free any memory allocated by the init functions.

**Arguments**:



**Returns**: 0 on success, -1 on failure.


<a                                                    name="stEvntMsk"><hr></a>

#### `int setEventMask(...)`

Configure which API events we are interested in.

This function will change the event mask to enable or disable
posting of API events. This can be called at any time, but if
events outside the mask have already been posted (but not handled)
they will not be not function.

See also: doc/Event_API.md

**Arguments**:

   * `int mask`: A bitmask describing which events we want

**Returns**: Always returns 0.


<a                                                       name="wtEvnt"><hr></a>

#### `int awaitEvent(...)`

Wait for a libpagekite event.

This function blocks until one of the libpagekite worker threads
posts an API event.

See also: doc/Event_API.md

**Arguments**:

   * `int timeout`: Max seconds to wait for an event

**Returns**: An integer code identifying the event.


<a                                                    name="gtEvntInt"><hr></a>

#### `int getEventInt(...)`

Get event data (integer).

This function returns the integer data associated with a given
API event.

See also: doc/Event_API.md

**Arguments**:

   * `int event_code`: The code identifying the event

**Returns**: An integer.


<a                                                    name="gtEvntStr"><hr></a>

#### `String getEventStr(...)`

Get event data (string).

This function returns a pointer to the data associated with a
given API event.

See also: doc/Event_API.md

**Arguments**:

   * `int event_code`: The code identifying the event

**Returns**: A pointer to a string.


<a                                                     name="vntRspnd"><hr></a>

#### `int eventRespond(...)`

Respond to a pagekite event.

Post a response to an API event.

 See also: doc/Event_API.md

**Arguments**:

   * `int event_code`: The event code
   * `int response_code`: Our response

**Returns**: Always returns 0.


<a                                                name="vntRspndWthDt"><hr></a>

#### `int eventRespondWithData(...)`

Respond to a pagekite event.

Post a response (with data) to an API event.

 See also: doc/Event_API.md

**Arguments**:

   * `int event_code`: The event code
   * `int response_code`: Our response
   * `int response_int`: Integer response data
   * `String response_str`: String response data (or NULL)

**Returns**: Always returns 0.


<a                                                       name="gtStts"><hr></a>

#### `int getStatus(...)`

Get the current status of the app.

This function can be called at any time.

**Arguments**:



**Returns**: A `PK_STATUS_*` code.


<a                                                         name="gtLg"><hr></a>

#### `String getLog(...)`

Fetch the in-memory log buffer.

Note that the C-API version returns a pointer to a static buffer.
Subsequent calls will overwrite with new data.

This function can be called at any time.

**Arguments**:



**Returns**: A snapshot of the current log status.


<a                                                    name="dmpSttTLg"><hr></a>

#### `int dumpStateToLog(...)`

Dump summary of internal state to log.

This function can be called at any time.

**Arguments**:



**Returns**: Always returns 0.


<a                                                          name="pll"><hr></a>

#### `int poll(...)`

Wait for the pagekite event loop to wake up.

This method can be used to pause a thread, waking up again when
libpagekite status changes, network activity takes place or other
events take place which might affect the log or state variable.

This method can be called any time the main thread is running.

**Arguments**:

   * `int timeout`: Max time to wait, in seconds

**Returns**: 0 on success, -1 if unconfigured.


<a                                                          name="tck"><hr></a>

#### `int tick(...)`

Manually trigger the internal tick event.

The internal tick event may trigger housekeeping if necessary.

This method is only needed if the internal periodic timer has
been disabled (e.g. on a mobile app, to conserve battery life).

This method can be called any time the main thread is running.

**Arguments**:



**Returns**: 0 on success, -1 if unconfigured.


<a                                                  name="stBlOnErrrs"><hr></a>

#### `int setBailOnErrors(...)`

Enable or disable bailing out on errors.

If enabled, the app will increase log verbosity and finally call
`exit(100)` after too many errors have occurred. This can help
catch and handle error states, but care should be taken as it
will also bring down the hosting app.

The sensitivity of this function depends on the `errors` variable.
After `errors * 9` problems would have been logged, verbosity
is increased. After `errors * 10`, the app will exit.

This function can be called at any time.

**Arguments**:

   * `int errors`: An error threshold

**Returns**: Always returns 0.


<a                                                         name="prrr"><hr></a>

#### `int perror(...)`

Log an error and reset the internal error state.

**Arguments**:

   * `String prefix`: Prefix for the logged message

**Returns**: Always returns 0.

<a                                                    name="constants"><hr></a>

## Constants

PageKiteAPI.PK_VERSION = "0.91.200718C"  
PageKiteAPI.PK_STATUS_STARTUP = 10  
PageKiteAPI.PK_STATUS_CONNECTING = 20  
PageKiteAPI.PK_STATUS_UPDATING_DNS = 30  
PageKiteAPI.PK_STATUS_FLYING = 40  
PageKiteAPI.PK_STATUS_PROBLEMS = 50  
PageKiteAPI.PK_STATUS_REJECTED = 60  
PageKiteAPI.PK_STATUS_NO_NETWORK = 90  
PageKiteAPI.PK_WITH_DEFAULTS = 0x8000  
PageKiteAPI.PK_WITHOUT_DEFAULTS = 0x4000  
PageKiteAPI.PK_WITH_SSL = 0x0001  
PageKiteAPI.PK_WITH_IPV4 = 0x0002  
PageKiteAPI.PK_WITH_IPV6 = 0x0004  
PageKiteAPI.PK_WITH_SERVICE_FRONTENDS = 0x0008  
PageKiteAPI.PK_WITHOUT_SERVICE_FRONTENDS = 0x0010  
PageKiteAPI.PK_WITH_DYNAMIC_FE_LIST = 0x0020  
PageKiteAPI.PK_WITH_FRONTEND_SNI = 0x0040  
PageKiteAPI.PK_WITH_SRAND_RESEED = 0x0080  
PageKiteAPI.PK_AS_FRONTEND_RELAY = 0x0100  
PageKiteAPI.PK_WITH_SYSLOG = 0x0200  
PageKiteAPI.PK_WITH_IPV4_DNS = 0x0400  
PageKiteAPI.PK_WITH_IPV6_DNS = 0x0800  
PageKiteAPI.PK_LOG_TUNNEL_DATA = 0x000100  
PageKiteAPI.PK_LOG_TUNNEL_HEADERS = 0x000200  
PageKiteAPI.PK_LOG_TUNNEL_CONNS = 0x000400  
PageKiteAPI.PK_LOG_BE_DATA = 0x001000  
PageKiteAPI.PK_LOG_BE_HEADERS = 0x002000  
PageKiteAPI.PK_LOG_BE_CONNS = 0x004000  
PageKiteAPI.PK_LOG_MANAGER_ERROR = 0x010000  
PageKiteAPI.PK_LOG_MANAGER_INFO = 0x020000  
PageKiteAPI.PK_LOG_MANAGER_DEBUG = 0x040000  
PageKiteAPI.PK_LOG_TRACE = 0x080000  
PageKiteAPI.PK_LOG_ERROR = 0x100000  
PageKiteAPI.PK_LOG_ERRORS = (PK_LOG_ERROR|PK_LOG_MANAGER_ERROR)  
PageKiteAPI.PK_LOG_MANAGER = (PK_LOG_MANAGER_ERROR|PK_LOG_MANAGER_INFO)  
PageKiteAPI.PK_LOG_CONNS = (PK_LOG_BE_CONNS|PK_LOG_TUNNEL_CONNS)  
PageKiteAPI.PK_LOG_NORMAL = (PK_LOG_ERRORS|PK_LOG_CONNS|PK_LOG_MANAGER)  
PageKiteAPI.PK_LOG_HEADERS = (PK_LOG_NORMAL|PK_LOG_TUNNEL_HEADERS|PK_LOG_BE_HEADERS)  
PageKiteAPI.PK_LOG_DATA = (PK_LOG_HEADERS|PK_LOG_TUNNEL_DATA|PK_LOG_BE_DATA)  
PageKiteAPI.PK_LOG_DEBUG = (PK_LOG_DATA|PK_LOG_MANAGER_DEBUG)  
PageKiteAPI.PK_LOG_ALL = 0xffff00  
PageKiteAPI.PK_LOG_DEST_SYSLOG = -1  
PageKiteAPI.PK_LOG_DEST_NONE = -2  
PageKiteAPI.PK_EV_ALL = 0xff000000  
PageKiteAPI.PK_EV_IS_BLOCKING = 0x80000000  
PageKiteAPI.PK_EV_PROCESSING = 0x40000000  
PageKiteAPI.PK_EV_MASK_ALL = 0x3f000000  
PageKiteAPI.PK_EV_MASK_LOGGING = 0x01000000  
PageKiteAPI.PK_EV_MASK_STATS = 0x02000000  
PageKiteAPI.PK_EV_MASK_CONN = 0x04000000  
PageKiteAPI.PK_EV_MASK__UNUSED__ = 0x08000000  
PageKiteAPI.PK_EV_MASK_DATA = 0x10000000  
PageKiteAPI.PK_EV_MASK_MISC = 0x20000000  
PageKiteAPI.PK_EV_SLOT_MASK = 0x00ff0000  
PageKiteAPI.PK_EV_SLOT_SHIFT = 16  
PageKiteAPI.PK_EV_SLOTS_MAX = 0x0100  
PageKiteAPI.PK_EV_TYPE_MASK = 0x3f00ffff  
PageKiteAPI.PK_EV_NONE = 0x00000000  
PageKiteAPI.PK_EV_SHUTDOWN = (0x00000001 | PK_EV_MASK_ALL)  
PageKiteAPI.PK_EV_LOGGING = (0x00000002 | PK_EV_MASK_LOGGING)  
PageKiteAPI.PK_EV_COUNTER = (0x00000003 | PK_EV_MASK_STATS)  
PageKiteAPI.PK_EV_CFG_FANCY_URL = (0x00000004 | PK_EV_MASK_MISC)  
PageKiteAPI.PK_EV_TUNNEL_REQUEST = (0x00000005 | PK_EV_MASK_MISC)  
PageKiteAPI.PK_EV_RESPOND_DEFAULT = 0x00000000  
PageKiteAPI.PK_EV_RESPOND_TRUE = 0x000000ff  
PageKiteAPI.PK_EV_RESPOND_OK = 0x00000001  
PageKiteAPI.PK_EV_RESPOND_ACCEPT = 0x00000002  
PageKiteAPI.PK_EV_RESPOND_FALSE = 0x0000ff00  
PageKiteAPI.PK_EV_RESPOND_ABORT = 0x00000100  
PageKiteAPI.PK_EV_RESPOND_REJECT = 0x00000200  