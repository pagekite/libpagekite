/* ****************************************************************************
   PageKiteAPI - Wrappers for the libpagekite API

      * * *   WARNING: This file is auto-generated, do not edit!  * * *

*******************************************************************************
This file is Copyright 2012-2017, The Beanstalks Project ehf.

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
    public static final String PK_VERSION = "0.91.170427C";
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
    public static final int PK_LOG_LUA_DEBUG = 0x008000;
    public static final int PK_LOG_LUA_INFO = 0x000800;
    public static final int PK_LOG_ERROR = 0x100000;
    public static final int PK_LOG_ERRORS = (PK_LOG_ERROR|PK_LOG_MANAGER_ERROR);
    public static final int PK_LOG_MANAGER = (PK_LOG_MANAGER_ERROR|PK_LOG_MANAGER_INFO);
    public static final int PK_LOG_CONNS = (PK_LOG_BE_CONNS|PK_LOG_TUNNEL_CONNS);
    public static final int PK_LOG_NORMAL = (PK_LOG_ERRORS|PK_LOG_CONNS|PK_LOG_MANAGER|PK_LOG_LUA_INFO);
    public static final int PK_LOG_DEBUG = (PK_LOG_NORMAL|PK_LOG_MANAGER_DEBUG|PK_LOG_LUA_DEBUG);
    public static final int PK_LOG_ALL = 0xffff00;

    public static native boolean init(String app_id, int max_kites, int max_frontends, int max_conns, String dyndns_url, int flags, int verbosity);
    public static native boolean initPagekitenet(String app_id, int max_kites, int max_conns, int flags, int verbosity);
    public static native boolean initWhitelabel(String app_id, int max_kites, int max_conns, int flags, int verbosity, String whitelabel_tld);
    public static native int addKite(String proto, String kitename, int pport, String secret, String backend, int lport);
    public static native int addServiceFrontends(int flags);
    public static native int addWhitelabelFrontends(int flags, String whitelabel_tld);
    public static native int lookupAndAddFrontend(String domain, int port, int update_from_dns);
    public static native int addFrontend(String domain, int port);
    public static native int setLogMask(int mask);
    public static native int setHousekeepingMinInterval(int interval);
    public static native int setHousekeepingMaxInterval(int interval);
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
    public static native int getStatus();
    public static native String getLog();
    public static native int dumpStateToLog();
    public static native int poll(int timeout);
    public static native int tick();
    public static native int setBailOnErrors(int errors);
    /* FIXME: void pagekite_perror(const char*) */
    /* FIXME: int pagekite_add_listener(const char* domain, int port, pagekite_callback_t* callback_func, void* callback_data) */
    /* FIXME: int pagekite_enable_lua_plugins(int enable_defaults, char** settings) */

    public static boolean libLoaded;
    static {
        try {
            System.loadLibrary("pagekite");
            libLoaded = true;
        } catch (Throwable t) { }
    }
}