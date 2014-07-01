using System;
using System.Runtime.InteropServices;

namespace PageKiteUI
{
    public class PkImport
    {
        private PkImport() {}

        [DllImport("PagekiteDLL.dll", EntryPoint = "pagekitec_init", CallingConvention = CallingConvention.Cdecl)]
        public static extern int pagekitec_init(int kites, int debug);

        [DllImport("PagekiteDLL.dll", EntryPoint = "pagekitec_addKite", CallingConvention = CallingConvention.Cdecl)]
        public static extern int pagekitec_addKite(
            [MarshalAs(UnmanagedType.LPStr)]string proto,
            [MarshalAs(UnmanagedType.LPStr)]string kitename, 
            int pport,
            [MarshalAs(UnmanagedType.LPStr)]string secret,
            [MarshalAs(UnmanagedType.LPStr)]string backend, 
            int lport
        );

        [DllImport("PagekiteDLL.dll", EntryPoint = "pagekitec_addFrontend", CallingConvention = CallingConvention.Cdecl)]
        public static extern int pagekitec_addFrontend(
            [MarshalAs(UnmanagedType.LPStr)]string domain, 
            int port
        );

        [DllImport("PagekiteDLL.dll", EntryPoint = "pagekitec_start", CallingConvention = CallingConvention.Cdecl)]
        public static extern int pagekitec_start();

        [DllImport("PagekiteDLL.dll", EntryPoint = "pagekitec_stop", CallingConvention = CallingConvention.Cdecl)]
        public static extern int pagekitec_stop();
    }
}