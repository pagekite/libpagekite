# libpagekite #

These are obsolete sections from the README.md - the need to be rewritten,
but are kept here to give hints and help folks get started in the meantime.

## Getting started on Windows ##

**WARNING:** This section is outdated and needs to be rewritten!

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

