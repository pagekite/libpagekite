This are INCOMPLETE build instructions for Windows.

## Cross-compiling to Windows

The libpagekite build process for Windows depends on the
[M cross environment](http://mxe.cc/). Here is a minimal example of how
to get up and running (omitting the MXE dependencies):

    $ cd /opt
    $ sudo git clone https://github.com/mxe/mxe.git
    ...
    $ cd mxe
    $ sudo make openssl pthread lua5.1
    ...

Then back in your libpagekite folder:

    $ ./tools/bash.mxe
    ...
    mxe> ./configure --host=i686-w64-mingw32.static \
                     --enable-static --disable-shared \
                     --without-java --without-os-libev \
                     --prefix=$(pwd)
    ...
    mxe> make
    ...

You should end up with a `pagekitec.exe` in `bin/`.


