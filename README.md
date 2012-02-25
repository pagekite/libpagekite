# libpagekite #

This will eventually be a tight, fast implementation of the PageKite protocol
in C, suitable for high-performance or embedded applications.

This code is a work in progress.


## What is PageKite? ##

PageKite is a protocol for dynamic, tunneled reverse proxying of arbitrary
TCP byte streams.  It is particularly well suited for making a HTTP server
on a device without a public IP address visible to the wider Internet, but
can also be used for a variety of other things, including SSH access.

For more information about PageKite, see http://pagekite.org/


## What's in the box? ##

Things that work:

    httpkite.c   Sample implementation of a very basic HTTP server
    pkproto.c    Parser and basic implementation of the protocol

Works in progress:

    pagekite.c   Standalone pagekite daemon.
    pkmanager.c  Management code: choose front-ends, re/connect, update DNS

Everything else is either support code or documentation.

