using System;
using System.Runtime.InteropServices;

namespace Pagekite
{
    public class PkImport
    {
        private PkImport() {}

        [DllImport("pkexport.dll", EntryPoint = "libpagekite_init", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libpagekite_init(int kites, int max_conns, int static_setup, 
                                                  int spare_frontends, int verbosity);
        //public static extern int libpagekite_init(int kites, int debug);

        [DllImport("pkexport.dll", EntryPoint = "libpagekite_useWatchdog", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libpagekite_useWatchdog();

        [DllImport("pkexport.dll", EntryPoint = "libpagekite_useEvil", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libpagekite_useEvil();

        [DllImport("pkexport.dll", EntryPoint = "libpagekite_addKite", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libpagekite_addKite(
            [MarshalAs(UnmanagedType.LPStr)]string proto,
            [MarshalAs(UnmanagedType.LPStr)]string kitename, 
            int pport,
            [MarshalAs(UnmanagedType.LPStr)]string secret,
            [MarshalAs(UnmanagedType.LPStr)]string backend, 
            int lport
        );

        [DllImport("pkexport.dll", EntryPoint = "libpagekite_addFrontend", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libpagekite_addFrontend(
            [MarshalAs(UnmanagedType.LPStr)]string domain, 
            int port
        );

        [DllImport("pkexport.dll", EntryPoint = "libpagekite_tick", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libpagekite_tick();

        [DllImport("pkexport.dll", EntryPoint = "libpagekite_start", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libpagekite_start();

        [DllImport("pkexport.dll", EntryPoint = "libpagekite_stop", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libpagekite_stop();

        [DllImport("pkexport.dll", EntryPoint = "libpagekite_getStatus", CallingConvention = CallingConvention.Cdecl)]
        public static extern int libpagekite_getStatus();

        [DllImport("pkexport.dll", EntryPoint = "libpagekite_getLog", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr libpagekite_getLog();

        //string ret = System.Runtime.InteropServices.Marshal.PtrToStringAnsi(PInvoke.ReturnString());
    }
}