# libpagekite #

This is a tight, fast implementation of the PageKite protocol in C,
suitable for high-performance or embedded applications.


## What is PageKite? ##

PageKite is a protocol for dynamic, tunneled reverse proxying of arbitrary
TCP byte streams. It is particularly well suited for making a HTTP server
on a device without a public IP address visible to the wider Internet, but
can also be used for a variety of other things, including SSH access.

For more information about PageKite, see http://pagekite.org/


## What is in the box? ##

The structure is as follows:

    Makefile            A few recipes for building common targets
    bindings/           Library bindings for other programming languages
    contrib/            Things that use libpagekite
    doc/                Documentation
    include/pagekite.h  The public interface of libpagekite
    libpagekite/        The source code
    tools/              Helper scripts for building and working

In contrib/backends/ you'll find:

    httpkite.c      A sample implementation of a very basic HTTP server
    pagekitec.c     Basic standalone pagekite back-end connector/proxy.


## Getting started ##

The best example of how to work with the library is `pagekitec.c`.

For more low-level fun, `httpkite.c` can also be of some value.

This project has a Wiki page: <https://pagekite.net/wiki/Floss/LibPageKite/>

To build on Linux, use:

    $ ./autogen.sh
    $ ./configure
    $ make

See `./configure --help` for some options.


## Getting started on Windows ##

Libpagekite can be cross-compiled from Linux to Windows by installing
the MXE environment, built with pthread and openssl at least.

You can the cross-compile Windows binaries and a DLL like so:

    $ make -f Makefile.pk windows

Check `docs/API.txt` for more details.

Releases include pre-built executables and DLLs in the `bin/` and `lib/`
folders for your convenience.


## Getting started on Android ##

**WARNING:** This section is outdated and needs to be rewritten!

This source tree can be included in an Android project using the NDK.  It
has been tested and verified to work with revision 8 of the NDK, targetting
Android 2.2 (Froyo, API level 8).

If PageKite is the only native package you are using, the quickest way to get
it to build as part of your project is by adding the following symbolic links
to your project tree:

    cd /path/to/YourApp/
    ln -s /path/to/libpagekite/ jni
    mkdir -p src/net/pagekite
    ln -s /path/to/libpagekite/net.pagekite.lib src/net/pagekite/lib

You will also need to grab a copy of OpenSSL for Android, we recommend the
version maintained by [the Guardian Project](https://guardianproject.info):

    cd /path/to/libpagekite/
    git clone https://github.com/guardianproject/openssl-android.git

<small>(Although not recommended, SSL support can be skipped by commenting out
the relevant lines in the `Android.mk` file and removing `#define HAVE_OPENSSL`
from `common.h`.)</small>

Finally, the JNI interface can then be built using the commands:

    cd /path/to/YourApp/
    export NDK_PROJECT_PATH=/path/to/android-ndk
    make -f jni/Makefile android

Expect this to take a while, as building OpenSSL for multiple architectures
is a pretty big task.  Once everything has been compiled, you should be able
to import `net.pagekite.lib` and use the methods of the `PageKiteAPI` class
in your app - but *please* read our licensing terms carefully if your app is
not Open Source.

If you are using multiple native packages, you may need to structure your
code differently and massage the Android.mk files a bit.


## License and Copyright ##

libpagekite is Copyright 2011-2015, The Beanstalks Project ehf.

This code is released under the Apache License 2.0, but may also be used
according to the terms of the GNU Affero General Public License.  Please
see the file COPYING.md for details on which license applies to you.

Commercial support for this code, as well as managed front-end relay service,
are available from <https://pagekite.net/>.

Development of this code was partially sponsored by
[SURFnet](http://www.surfnet.nl) and the [Icelandic Technology Development
fund](http://www.rannis.is/).

