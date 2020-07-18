/* ****************************************************************************
   PageKiteAPI - Wrappers for the libpagekite API

      * * *   WARNING: This file is auto-generated, do not edit!  * * *

*******************************************************************************
This file is Copyright 2012-2020, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify it under
the terms  of the  Apache  License 2.0  as published by the  Apache  Software
Foundation.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the Apache License for more details.

You should have received a copy of the Apache License along with this program.
If not, see: <http://www.apache.org/licenses/>

Note: For alternate license terms, see the file COPYING.md.
**************************************************************************** */

package net.pagekite.lib;

public class PageKiteAPI extends Object
{
    public static final String PK_VERSION = "0.91.200718C";
    public static final int PK_STATUS_STARTUP = 10;
    public static final int PK_STATUS_CONNECTING = 20;
    public static final int PK_STATUS_UPDATING_DNS = 30;
    public static final int PK_STATUS_FLYING = 40;
    public static final int PK_STATUS_PROBLEMS = 50;
    public static final int PK_STATUS_REJECTED = 60;
    public static final int PK_STATUS_NO_NETWORK = 90;
    public static final int PK_WITH_DEFAULTS = 0x8000;
    public static final int PK_WITHOUT_DEFAULTS = 0x4000;
    public static final int PK_WITH_SSL = 0x0001;
    public static final int PK_WITH_IPV4 = 0x0002;
    public static final int PK_WITH_IPV6 = 0x0004;
    public static final int PK_WITH_SERVICE_FRONTENDS = 0x0008;
    public static final int PK_WITHOUT_SERVICE_FRONTENDS = 0x0010;
    public static final int PK_WITH_DYNAMIC_FE_LIST = 0x0020;
    public static final int PK_WITH_FRONTEND_SNI = 0x0040;
    public static final int PK_WITH_SRAND_RESEED = 0x0080;
    public static final int PK_AS_FRONTEND_RELAY = 0x0100;
    public static final int PK_WITH_SYSLOG = 0x0200;
    public static final int PK_WITH_IPV4_DNS = 0x0400;
    public static final int PK_WITH_IPV6_DNS = 0x0800;
    public static final int PK_LOG_TUNNEL_DATA = 0x000100;
    public static final int PK_LOG_TUNNEL_HEADERS = 0x000200;
    public static final int PK_LOG_TUNNEL_CONNS = 0x000400;
    public static final int PK_LOG_BE_DATA = 0x001000;
    public static final int PK_LOG_BE_HEADERS = 0x002000;
    public static final int PK_LOG_BE_CONNS = 0x004000;
    public static final int PK_LOG_MANAGER_ERROR = 0x010000;
    public static final int PK_LOG_MANAGER_INFO = 0x020000;
    public static final int PK_LOG_MANAGER_DEBUG = 0x040000;
    public static final int PK_LOG_TRACE = 0x080000;
    public static final int PK_LOG_ERROR = 0x100000;
    public static final int PK_LOG_ERRORS = (PK_LOG_ERROR|PK_LOG_MANAGER_ERROR);
    public static final int PK_LOG_MANAGER = (PK_LOG_MANAGER_ERROR|PK_LOG_MANAGER_INFO);
    public static final int PK_LOG_CONNS = (PK_LOG_BE_CONNS|PK_LOG_TUNNEL_CONNS);
    public static final int PK_LOG_NORMAL = (PK_LOG_ERRORS|PK_LOG_CONNS|PK_LOG_MANAGER);
    public static final int PK_LOG_HEADERS = (PK_LOG_NORMAL|PK_LOG_TUNNEL_HEADERS|PK_LOG_BE_HEADERS);
    public static final int PK_LOG_DATA = (PK_LOG_HEADERS|PK_LOG_TUNNEL_DATA|PK_LOG_BE_DATA);
    public static final int PK_LOG_DEBUG = (PK_LOG_DATA|PK_LOG_MANAGER_DEBUG);
    public static final int PK_LOG_ALL = 0xffff00;
    public static final int PK_LOG_DEST_SYSLOG = -1;
    public static final int PK_LOG_DEST_NONE = -2;
    public static final int PK_EV_ALL = 0xff000000;
    public static final int PK_EV_IS_BLOCKING = 0x80000000;
    public static final int PK_EV_PROCESSING = 0x40000000;
    public static final int PK_EV_MASK_ALL = 0x3f000000;
    public static final int PK_EV_MASK_LOGGING = 0x01000000;
    public static final int PK_EV_MASK_STATS = 0x02000000;
    public static final int PK_EV_MASK_CONN = 0x04000000;
    public static final int PK_EV_MASK__UNUSED__ = 0x08000000;
    public static final int PK_EV_MASK_DATA = 0x10000000;
    public static final int PK_EV_MASK_MISC = 0x20000000;
    public static final int PK_EV_SLOT_MASK = 0x00ff0000;
    public static final int PK_EV_SLOT_SHIFT = 16;
    public static final int PK_EV_SLOTS_MAX = 0x0100;
    public static final int PK_EV_TYPE_MASK = 0x3f00ffff;
    public static final int PK_EV_NONE = 0x00000000;
    public static final int PK_EV_SHUTDOWN = (0x00000001 | PK_EV_MASK_ALL);
    public static final int PK_EV_LOGGING = (0x00000002 | PK_EV_MASK_LOGGING);
    public static final int PK_EV_COUNTER = (0x00000003 | PK_EV_MASK_STATS);
    public static final int PK_EV_CFG_FANCY_URL = (0x00000004 | PK_EV_MASK_MISC);
    public static final int PK_EV_TUNNEL_REQUEST = (0x00000005 | PK_EV_MASK_MISC);
    public static final int PK_EV_RESPOND_DEFAULT = 0x00000000;
    public static final int PK_EV_RESPOND_TRUE = 0x000000ff;
    public static final int PK_EV_RESPOND_OK = 0x00000001;
    public static final int PK_EV_RESPOND_ACCEPT = 0x00000002;
    public static final int PK_EV_RESPOND_FALSE = 0x0000ff00;
    public static final int PK_EV_RESPOND_ABORT = 0x00000100;
    public static final int PK_EV_RESPOND_REJECT = 0x00000200;

    public static native boolean init(String app_id, int max_kites, int max_frontends, int max_conns, String dyndns_url, int flags, int verbosity);
    public static native boolean initPagekitenet(String app_id, int max_kites, int max_conns, int flags, int verbosity);
    public static native boolean initWhitelabel(String app_id, int max_kites, int max_conns, int flags, int verbosity, String whitelabel_tld);
    public static native int addKite(String proto, String kitename, int pport, String secret, String backend, int lport);
    public static native int addServiceFrontends(int flags);
    public static native int addWhitelabelFrontends(int flags, String whitelabel_tld);
    public static native int lookupAndAddFrontend(String domain, int port, int update_from_dns);
    public static native int addFrontend(String domain, int port);
    public static native int setLogMask(int mask);
    public static native int setLogDestination(int log_destination);
    public static native int setHousekeepingMinInterval(int interval);
    public static native int setHousekeepingMaxInterval(int interval);
    public static native int setRejectionUrl(String url);
    public static native int enableHttpForwardingHeaders(int enable);
    public static native int enableFakePing(int enable);
    public static native int enableWatchdog(int enable);
    public static native int enableTickTimer(int enable);
    public static native int setConnEvictionIdleS(int seconds);
    public static native int setOpensslCiphers(String ciphers);
    public static native int wantSpareFrontends(int spares);
    public static native int threadStart();
    public static native int threadWait();
    public static native int threadStop();
    public static native int free();
    public static native int setEventMask(int mask);
    public static native int awaitEvent(int timeout);
    public static native int getEventInt(int event_code);
    public static native String getEventStr(int event_code);
    public static native int eventRespond(int event_code, int response_code);
    public static native int eventRespondWithData(int event_code, int response_code, int response_int, String response_str);
    public static native int getStatus();
    public static native String getLog();
    public static native int dumpStateToLog();
    public static native int poll(int timeout);
    public static native int tick();
    public static native int setBailOnErrors(int errors);
    public static native int perror(String prefix);

    public static boolean libLoaded;
    static {
        try {
            System.loadLibrary("pagekite");
            libLoaded = true;
        } catch (Throwable t) { }
    }
}