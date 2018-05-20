# libpagekite Event API

   * [Introduction                           ](#intro)
   * [Python Interface (high level)          ](#python)
   * [Generic Public Interface (low level)   ](#generic)
   * [libpagekite Internal Interface         ](#internal)


<a                                                      name="intro"><hr></a>

## Introduction

This document describes the libpagekite Event API.

The libpagekite Event API is a message-based callback API which allows a
running libpagekite to inform (send log data, statistics or introspection)
or offload processing to (callbacks), the caller.

(**Warning:** Unlike [API.md](API.md), this document is not currently
auto-generated from the source and is somewhat more likely to become out
of date. When in doubt, check the code!)


<a                                                     name="python"><hr></a>

## Python Interface (high level)

[This class](../bindings/python/libpagekite/events.py) provides an
object-oriented method for defining and handling events; you subclass
`libpagekite.events.BaseEventHandler` and override the methods
representing the callbacks you are interested in.

See: `cd bindings/python; pydoc libpagekite/events.py`.


<a                                                    name="generic"><hr></a>

## Generic Public Interface (low level)

A queue of events is used as a coordination point between threads, so
consumers of the API don't need to understand libpagekite's internal
threading. This also allows the caller to themself be multithreaded.

Events are identified using basic primitives (integers and strings) to
facilitate cross-platform API compatibility.

So a consumer of the API just needs to follow this pattern:

    int timeout = 5;                     // Seconds
    pagekite_set_event_mask(PK_EV_ALL);  // Or maybe not all...
    unsigned int event_code;
    while (PK_EV_SHUTDOWN != (
      (event_code = (pagekite_await_event(pkm, timeout))
                     & PK_EV_TYPE_MASK)))
    {
      switch (event_code & PK_EV_TYPE_MASK) {
 
        case PK_EV_FANCYCODE:
          pagekite_event_respond_with_data(pkm, event_code,
                                           PK_EV_RESPOND_AWESOME,
                                           reponse_integer,
                                           response_string);

        ...

        default:
          pagekite_event_respond(pkm, event_code,
                                 PK_EV_RESPOND_DEFAULT);
      }
    }

Suppressing certain classes of events using the event mask will squelch
the events at their "source", thus reducing lock contention and overall
work.

#### Further reading

See also API.md for details on individual API calls:

   * [`pagekite_set_event_mask              `](API.md#pgktstvntmsk)
   * [`pagekite_await_event                 `](API.md#pgktwtvnt)
   * [`pagekite_get_event_int               `](API.md#pgktgtvntnt)
   * [`pagekite_get_event_str               `](API.md#pgktgtvntstr)
   * [`pagekite_event_respond               `](API.md#pgktvntrspnd)
   * [`pagekite_event_respond_with_data     `](API.md#pgktvntrspndwthdt)

See [`pagekite.h`](../include/pagekite.h) or [API.md](API.md) for the full
list of defined `PK_EV_...` constants.

The Python [`events.py`](../bindings/python/libpagekite/events.py) module
contains a readable example of how to use the Events API.


<a                                                   name="internal"><hr></a>

## libpagekite Internal Interface

[TODO: WRITE THIS]


