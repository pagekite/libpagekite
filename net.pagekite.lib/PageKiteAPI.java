/******************************************************************************
PageKiteAPI - A high-performance PageKite implementation in C.

*******************************************************************************

This file is Copyright 2011, 2012, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify it under
the terms of the  GNU  Affero General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for more
details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see: <http://www.gnu.org/licenses/>

******************************************************************************/
package net.pagekite.lib;

public class PageKiteAPI extends Object
{
    public static final int PK_STATUS_STARTUP     = 10;
    public static final int PK_STATUS_CONNECT     = 20;
    public static final int PK_STATUS_DYNDNS      = 30;
    public static final int PK_STATUS_FLYING      = 40;
    public static final int PK_STATUS_PROBLEMS    = 50;
    public static final int PK_STATUS_REJECTED    = 60;
    public static final int PK_STATUS_NO_NETWORK  = 90;

    /* Direct wrapper around library init function. */
    public static native boolean init(int jKites, int jFrontends, int jConns,
                                      String jDynDns, boolean jDebug);

    /* Initialize library using PageKite service defaults. */
    public static native boolean initPagekiteNet(int jKites, int jConns, boolean jDebug);

    public static native boolean addKite(String jProto, String jKitename,
                                         int jPort, String jSecret,
                                         String jBackend, int jLport);
    public static native boolean addFrontend(String jKitename, int jPort);

    public static native boolean start();
    public static native boolean stop();

    public static native int poll(int timeout);
    public static native int getStatus();
    public static native int getLiveFrontends();
    public static native int getLiveStreams();
    public static native String getLog();

    static {
        System.loadLibrary("pagekite-jni");
    }
}
