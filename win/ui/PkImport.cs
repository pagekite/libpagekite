using System;
using System.Runtime.InteropServices;

namespace PageKiteUI
{
    public class PkImport
    {
        private PkImport() {}

        [DllImport("PagekiteDLL.dll", EntryPoint = "libpagekite_init", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libpagekite_init(int kites, int debug);

        [DllImport("PagekiteDLL.dll", EntryPoint = "libpagekite_useWatchdog", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libpagekite_useWatchdog();

        [DllImport("PagekiteDLL.dll", EntryPoint = "libpagekite_useEvil", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libpagekite_useEvil();

        [DllImport("PagekiteDLL.dll", EntryPoint = "libpagekite_addKite", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libpagekite_addKite(
            [MarshalAs(UnmanagedType.LPStr)]string proto,
            [MarshalAs(UnmanagedType.LPStr)]string kitename, 
            int pport,
            [MarshalAs(UnmanagedType.LPStr)]string secret,
            [MarshalAs(UnmanagedType.LPStr)]string backend, 
            int lport
        );

        [DllImport("PagekiteDLL.dll", EntryPoint = "libpagekite_addFrontend", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libpagekite_addFrontend(
            [MarshalAs(UnmanagedType.LPStr)]string domain, 
            int port
        );

        [DllImport("PagekiteDLL.dll", EntryPoint = "libpagekite_start", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libpagekite_start();

        [DllImport("PagekiteDLL.dll", EntryPoint = "libpagekite_stop", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libpagekite_stop();

        [DllImport("PagekiteDLL.dll", EntryPoint = "libpagekite_getStatus", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libpagekite_getStatus();

        [DllImport("PagekiteDLL.dll", EntryPoint = "libpagekite_getLog", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr libpagekite_getLog();

        //string ret = System.Runtime.InteropServices.Marshal.PtrToStringAnsi(PInvoke.ReturnString());
    }
}