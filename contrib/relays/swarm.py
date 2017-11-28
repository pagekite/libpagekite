#!/usr/bin/python
#
# swarm: High performance PageKite connector and relay. Swarms of pagekites!
#
# This app applies the pagekite.py configuration logic to the high performance
# libpagekite library.
#
import sys
import pagekite
import pagekite.pk
import pagekite.httpd
import pagekite.common as pk_common
from libpagekite import *
from libpagekite.events import BaseEventHandler


class ConfigError(Exception):
    pass


class EventHandler(BaseEventHandler):
    def __init__(self, lpk, config):
        BaseEventHandler.__init__(self, lpk)
        self.config = config

    def debug(self, message):
        print 'Woo: %s' % message

    def fancy_rejection_url(self, event_id):
        if self.config.error_urls:
            request_host = self.get_event_string(event_id)
            dparts = request_host.split('.')
            while dparts:
                eu = self.config.error_urls.get('.'.join(dparts), None)
                if eu is not None:
                    return (PK_EV_RESPOND_DEFAULT, 0, eu)
                dparts.pop(0)
        if self.config.error_url:
            return (PK_EV_RESPOND_DEFAULT, 0, self.config.error_url)
        return PK_EV_RESPOND_DEFAULT

    def tunnel_request(self, event_id):
        request_data = self.get_event_string(event_id)       
        # FIXME...
        return (PK_EV_RESPOND_REJECT, 1, "X-PageKite-WTF: Sorry!\r\n\r\n")


def PageKiteUiClass():
    if hasattr(sys.stdout, 'isatty') and sys.stdout.isatty():
        import pagekite.ui.basic
        return pagekite.ui.basic.BasicUi
    else:
        import pagekite.ui.nullui
        return pagekite.ui.nullui.NullUi


def InitFromPK(pk):
    flags = (
        PK_WITH_SSL |
        PK_WITH_IPV4 |
        PK_WITH_IPV6 |
        PK_WITH_SRAND_RESEED)

    if pk.isfrontend:
        flags |= PK_AS_FRONTEND_RELAY

    # FIXME: This only works for pagekite.net-like DDNS servers
    if pk.dyndns and pk.dyndns[0]:
        dyndns = pk.dyndns[0].split('?')[0] + '?hostname=%s&myip=%s&sign=%s'
    else:
        dyndns = ''

    # FIXME: Numbers should adapt to configuration
    # FIXME: Debug mask should adapt to config
    return ['PageKiteSwarm', 50, 50, 100, dyndns, flags, PK_LOG_ALL]


def KitesFromPK(pk):
    # FIXME
    for bid, info in pk.backends.iteritems():
        if (info[pk_common.BE_STATUS] != pk_common.BE_STATUS_DISABLED
                and info[pk_common.BE_BHOST]
                and info[pk_common.BE_BPORT] > 0):
            yield [
                info[pk_common.BE_PROTO],
                info[pk_common.BE_DOMAIN],
                info[pk_common.BE_PORT] or 0,
                info[pk_common.BE_SECRET],
                info[pk_common.BE_BHOST],
                info[pk_common.BE_BPORT]]


def InitLibpagekiteFromPK(pk):
    init = InitFromPK(pk)
    lpk = PageKite().init(*init)
    with lpk:
        for kite in KitesFromPK(pk):
            if 0 <= lpk.add_kite(*kite):
                print 'Added kite: %s' % kite
                if not pk.servers_new_only:
                    if 0 > lpk.add_frontend(kite[1], kite[2]):
                        lpk.perror(sys.argv[0])
            else:
                lpk.perror(sys.argv[0])
                raise ConfigError('add_relay_listener')

        if pk.isfrontend:
            print 'Frontend wanted, woooo! flags=0x%x' % init[3]
            for port in pk.server_ports:
                if 0 <= lpk.add_relay_listener(int(port), init[3]):
                    print 'Listening on: %s' % port
                else:
                    lpk.perror(sys.argv[0])
                    raise ConfigError('add_relay_listener')

        lpk.set_event_mask(PK_EV_MASK_ALL)

        return lpk


def Main():
    config = pagekite.pk.PageKite(
        ui=PageKiteUiClass(),
        http_handler=pagekite.httpd.UiRequestHandler,
        http_server=pagekite.httpd.UiHttpServer)

    pagekite.pk.Configure(config)
    with InitLibpagekiteFromPK(config) as lpk:
        lpk.thread_start()
        EventHandler(lpk, config).handle_events()
        lpk.thread_stop()


if __name__ == '__main__':
    Main()
