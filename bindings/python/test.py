#!/usr/bin/python
#
## This is a (incomplete) libpagekite wrapper in Python
#
#     $ LD_LIBRARY_PATH=. python libpagekite.py
#
from ctypes import *


LIBPK = cdll.LoadLibrary('libpagekite.so')

c_M, c_S, c_I = c_void_p, c_char_p, c_int
LIBPK_METHODS = ( 
    (c_M, 'init',             (c_S, c_I, c_I, c_I, c_S, c_I, c_I)),
    (c_M, 'init_pagekitenet', (c_S, c_I, c_I, c_I, c_I)),
    (c_M, 'init_whitelabel',  (c_S, c_I, c_I, c_I, c_I, c_S)),
    (c_I, 'add_kite',         (c_M, c_S, c_S, c_I, c_S, c_S, c_I)),
    (c_I, 'thread_start',     (c_M,)),
    (c_I, 'thread_wait',      (c_M,)),
    (c_I, 'thread_stop',      (c_M,)),
    (c_I, 'free',             (c_M,)))

for restype, func_name, argtypes in LIBPK_METHODS:
    method = getattr(LIBPK, 'pagekite_%s' % func_name)
    method.restype = restype
    method.argtypes = argtypes
    

class PageKite(object):
    def __init__(self):
        self.pkm = None 

        def wrapped(restype, func_name):
            method = getattr(LIBPK, 'pagekite_%s' % func_name)
            if restype == c_M:
                def wrapper(*args):
                    assert(not self.pkm)
                    self.pkm = method(*args)
                    return bool(self.pkm)
            else:
                def wrapper(*args):
                    assert(self.pkm)
                    return method(self.pkm, *args)
            return wrapper

        for restype, func_name, argtypes in LIBPK_METHODS:
            setattr(self, func_name, wrapped(restype, func_name))


if __name__ == '__main__':
    import sys, time

    pk = PageKite()
    pk.init_pagekitenet('Testing', 25, 25, 0, 0)
    try:
        pk.add_kite(sys.argv[2],      # proto
                    sys.argv[3],      # kitename
                    0,                # protocol
                    sys.argv[4],      # secret
                    'localhost',
                    int(sys.argv[1])) # local port
        pk.thread_start()
        pk.thread_wait()
        pk.thread_stop()
    finally:
        pk.free()
