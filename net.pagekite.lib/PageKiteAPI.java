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
    public static native boolean init(int jKites, int jFrontend, int jConns);
    public static native boolean free();

    public static native boolean addKite(String jProto, String jKitename,
    		                         	 int jPort, String jSecret,
    		                         	 String jBackend, int jLport);
    public static native boolean addFrontend(String jKitename, int jPort, int jPrio);

    public static native boolean start();
    public static native boolean stop();

    static {
        System.loadLibrary("pagekite-jni");
    }
}
