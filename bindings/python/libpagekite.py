# This is a (incomplete) libpagekite wrapper in Python
#
#     $ LD_LIBRARY_PATH=. python libpagekite.py
#
from ctypes import *
import time


LIBPK = cdll.LoadLibrary('libpagekite.so')
LIBPK.pks_global_init(0xFFFF)                 # PK_LOG_NORMAL == 0x1344


def voidp(fnc):
    fnc.restype = c_void_p
    return fnc

NULL = POINTER(c_int)()


class PageKite(object):
    def __init__(self):
        self.kites = []

    def add_kite(self, lport, proto, kitename, pport, secret):
        self.kites.append((lport, proto, kitename, pport, secret))
        return self

    def fly(self):
        self.pkm = voidp(LIBPK.pkm_manager_init)(NULL, 0, NULL,
                                                 5, 25, 25, NULL, NULL)
        for (lport, proto, kitename, pport, secret) in self.kites:
            LIBPK.pkm_add_kite(self.pkm, proto, kitename, 0, secret,
                                         'localhost', lport)
            LIBPK.pkm_add_frontend(self.pkm, kitename, pport, 0)
        LIBPK.pkm_run_in_thread(self.pkm)

    def land(self):
        LIBPK.pkm_stop_thread(self.pkm)


if __name__ == '__main__':
    pk = PageKite()
    try:
        pk.add_kite(80, 'http', 'FIXME.pagekite.me', 443, 'FIXME').fly()
        time.sleep(60)
    finally:
        pk.land()
