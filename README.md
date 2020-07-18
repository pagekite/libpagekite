# libpagekite #

This is a tight, fast implementation of the PageKite protocol in C,
suitable for high-performance or embedded applications.


## What is PageKite? ##

PageKite is a protocol for dynamic, tunneled reverse proxying of arbitrary
TCP byte streams. It is particularly well suited for making a HTTP server
on a device without a public IP address visible to the wider Internet, but
can also be used for a variety of other things, including SSH access.

PageKite is usually used with the public relay service provided by
<https://pagekite.net/>, but you can also run your own relay if you prefer;
the [Python relay is free software](https://github.com/pagekite/PyPagekite/).

For more information, see <http://pagekite.org/>


## What is in the box? ##

The structure is as follows:

    bindings/           Library bindings for Java, Python
    contrib/            Things that use libpagekite
    doc/                Documentation
    include/pagekite.h  The public interface of libpagekite
    java-classes/       Compiled Java classes
    libpagekite/        The source code
    tools/              Helper scripts for building and working

In `contrib/backends/` you'll find:

    httpkite.c         A sample implementation of a very basic HTTP server
    pagekitec.c        Basic standalone pagekite back-end connector/proxy.
    sshkite.c          An example of exposing SSH over pagekite.
    PageKiteTest.java  Minimal Java test connector


## Getting started ##

Be sure you have the following packages (list assumes Debian/Ubuntu, your
distro will offer something equivalent):

    $ apt install build-essential pkg-config libtool automake
    $ apt install libev-dev libssl-dev

To build for development on Linux, use:

    $ ./autogen.sh
    $ ./configure --prefix=$(pwd) --without-java
    $ make install

This will build the library (without Java bindings), placing binaries in
the `bin/` and `lib/` subdirectories. You can then run the test connector
like so:

    $ export LD_LIBRARY_PATH=$(pwd)/lib
    $ ./bin/pagekitec 80 http yourkite.pagekite.me 0 kitesecret

To run the Python test:

    $ export LD_LIBRARY_PATH=$(pwd)/lib
    $ cd bindings/python
    $ export PYTHONPATH="$(pwd):$PYTHONPATH"
    $ python ./cython_demo.py 80 http yourkite.pagekite.me secret

To run the Java test:

    $ ./configure --prefix=$(pwd)
    $ make install
    $ export LD_LIBRARY_PATH=$(pwd)/lib
    $ cd java-classes
    $ java PageKiteTest

*(Note: The Java test is expected to fail because of hard-coded invalid
credentials. You'll need to edit the source for it to actually work.)*

Press `CTRL+C` to exit any of the test apps.


## Documentation and examples

See `./configure --help` for some options on how to build the library.

   * [The C API reference        ](doc/API.md)
   * [The Java/JNI API reference ](doc/API_JNI.md)
   * [The C backend connector    ](contrib/backends/pagekitec.c)
   * [The Java example           ](contrib/backends/PageKiteTest.java)
   * See `pydoc bindings/python/libpagekite/__init__.py` for Python docs

See [the old README](doc/README-OLD.md) for hints on how some things used
to be done and may still be missing from our current build processes and
documentation.


## State of the Onion

As of February 2017, the library's main limitations are:

   * [SSL certificates are not verified when connecting to the
      relay](https://github.com/pagekite/libpagekite/issues/6)
   * The front-end relay code is incomplete
   * Cross-compiling to Windows with autoconf is untested

The back-end connector code is however considered mature and stable; the
library does a good job connecting, relaying traffic and *reconnecting* as
necessary. Stability has been pretty good, but [check our issues for the
latest bug reports](https://github.com/pagekite/libpagekite/issues?utf8=%E2%9C%93&q=is%3Aissue)
and [the commit log for recent fixes](https://github.com/pagekite/libpagekite/commits/master).


## License and Copyright ##

libpagekite is Copyright 2011-2020, The Beanstalks Project ehf.

This code is released under the Apache License 2.0, but may also be used
according to the terms of the GNU Affero General Public License.  Please
see the file [COPYING.md](doc/COPYING.md) for details on which license applies
to you.

Commercial support for this code, as well as managed front-end relay service,
are available from <https://pagekite.net/>.

Development of this code was partially sponsored by
[SURFnet](http://www.surfnet.nl) and the [Icelandic Technology Development
fund](http://www.rannis.is/).

