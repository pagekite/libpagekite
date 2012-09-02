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


## What is in the box? ##

Things that work:

    httpkite.c     Sample implementation of a very basic HTTP server
    pkproto.c      Parser and basic implementation of the protocol

Works in progress:

    pagekite.c     Standalone pagekite daemon.
    pkmanager.c    Management code: choose front-ends, re/connect, update DNS
    pagekite-jni.c JNI wrapper for use in Android development

Everything else is either support code or documentation.


## Getting started ##

Playing with `httpkite.c` is probably the easiest way to get started.  You
can build it like so:

    $ make httpkite

Running the program will give some hints on how to use it.  It does not do
anything useful on its own, the idea is to give hackers a basic implementation
which they can extend and build upon.

This project has a Wiki page: <https://pagekite.net/wiki/Floss/LibPageKite/>


## Getting started on Android ##

This source tree can be included in an Android project using the NDK.  It
has been tested with revision 8 of the NDK.

If PageKite is the only native package you are using, the quickest way to get
it to build as part of your project is by adding the following symbolic links
to your project tree:

   cd /path/to/YourApp/
   ln -s /path/to/libpagekite/ jni
   mkdir -p src/net/pagekite
   ln -s /path/to/libpagekite/net.pagekite.lib src/net/pagekite/lib

The JNI interface can then be built using the commands:

   export NDK_PROJECT_PATH=/path/to/android-ndk
   make -f jni/Makefile android

Once this is done, you should be able to import net.pagekite.lib and use
the methods of the PageKiteAPI class.

If you are using multiple native packages, you may need to structure your
code differently and massage the Android.mk files a bit.


## License and Copyright ##

libpagekite is Copyright 2011, 2012, The Beanstalks Project ehf.

This code is released under the GNU Affero General Public License version 3,
but may also be used (under specific conditions) according to the terms of the
GNU Lesser General Public License.  Please see the file COPYING.md for details
on which license applies to you.

Commercial support for this code, as well as managed front-end relay service,
are available from <https://pagekite.net/>.

Development of this code was partially sponsored by
[SurfNet](http://www.surfnet.nl) and the [Icelandic Technology Development
fund](http://www.rannis.is/).

