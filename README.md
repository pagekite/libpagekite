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


## Getting started ##

Playing with `httpkite.c` is probably the easiest way to get started.  You
can build it like so:

    $ make httpkite

Running the program will give some hints on how to use it.  It does not do
anything useful on its own, the idea is to give hackers a basic implementation
which they can extend and build upon.

This project has a Wiki page: <https://pagekite.net/wiki/Floss/LibPageKite/>


## License and Copyright ##

libpagekite is Copyright 2011, 2012, The Beanstalks Project ehf.

**Note:** Alternate licenses and commercial support for this code, as well
as managed front-end service, are available from <https://pagekite.net/>.

This program is free software: you can redistribute it and/or modify it under
the terms of the  GNU  Affero General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for more
details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see: <http://www.gnu.org/licenses/>

