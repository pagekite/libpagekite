using System;
using System.Runtime.InteropServices;

namespace Pagekite
{
    public class PkLib
    {
        /* Flags for pagekite_init and friends - replicated from pagekite.h */
        public const int PK_WITH_DEFAULTS             = 0x0000;
        public const int PK_WITHOUT_DEFAULTS          = 0x1000;
        public const int PK_WITH_SSL                  = 0x0001;
        public const int PK_WITH_IPV4                 = 0x0002;
        public const int PK_WITH_IPV6                 = 0x0004;
        public const int PK_WITH_SERVICE_FRONTENDS    = 0x0008;
        public const int PK_WITHOUT_SERVICE_FRONTENDS = 0x0010;

        public const string DLL = "libpagekite.dll";
        private IntPtr pkm;

        public PkLib() {
            this.pkm = new IntPtr(0);
        }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr pagekite_init(
            [MarshalAs(UnmanagedType.LPStr)]string app_id,
            int max_kites,
            int max_frontends,
            int max_conns,
            [MarshalAs(UnmanagedType.LPStr)]string dyndns_url,
            int flags,
            int verbosity);
        public bool init(string app_id, int max_kites, int max_frontends,
                         int max_conns, string dyndns_url, int flags,
                         int verbosity) {
            this.pkm = pagekite_init(app_id,
                                     max_kites, max_frontends, max_conns,
                                     dyndns_url, flags, verbosity);
            return (this.pkm != IntPtr.Zero);
        }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr pagekite_init_pagekitenet(
            [MarshalAs(UnmanagedType.LPStr)]string app_id,
            int max_kites,
            int max_conns,
            int flags,
            int verbosity);
        public bool init_pagekitenet(string app_id, int max_kites,
                                     int max_conns, int flags, int verbosity) {
            this.pkm = pagekite_init_pagekitenet(app_id, max_kites, max_conns,
                                                 flags, verbosity);
            return (this.pkm != IntPtr.Zero);
        }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int pagekite_add_service_frontends(IntPtr pkm,
            int flags);
        public int add_service_frontends(int flags) {
            return pagekite_add_service_frontends(this.pkm, flags); }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int pagekite_free(IntPtr pkm);
        // FIXME: Need to auto-invoke this in destructor!
        public int free() {
            int rv = pagekite_free(this.pkm);
            this.pkm = new IntPtr(0);
            return rv;
        }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int pagekite_set_log_mask(IntPtr pkm,
            int mask);
        public int set_log_mask(int mask) {
            return pagekite_set_log_mask(this.pkm, mask); }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int pagekite_enable_watchdog(IntPtr pkm,
            int enable);
        public int enable_watchdog(int enable) {
            return pagekite_enable_watchdog(this.pkm, enable); }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int pagekite_enable_fake_ping(IntPtr pkm,
            int enable);
        public int enable_fake_ping(int enable) {
            return pagekite_enable_fake_ping(this.pkm, enable); }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int pagekite_set_bail_on_errors(IntPtr pkm,
            int errors);
        public int set_bail_on_errors(int errors) {
            return pagekite_set_bail_on_errors(this.pkm, errors); }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int pagekite_set_conn_eviction_idle_s(IntPtr pkm,
            int seconds);
        public int set_conn_eviction_idle_s(int seconds) {
            return pagekite_set_conn_eviction_idle_s(this.pkm, seconds); }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int pagekite_want_spare_frontends(IntPtr pkm,
            int spares);
        public int want_spare_frontends(int spares) {
            return pagekite_want_spare_frontends(this.pkm, spares); }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int pagekite_add_kite(IntPtr pkm,
            [MarshalAs(UnmanagedType.LPStr)]string proto,
            [MarshalAs(UnmanagedType.LPStr)]string kitename,
            int pport,
            [MarshalAs(UnmanagedType.LPStr)]string secret,
            [MarshalAs(UnmanagedType.LPStr)]string backend,
            int lport);
        public int add_kite(string proto, string kitename, int pport,
                            string secret, string backend, int lport) {
            return pagekite_add_kite(this.pkm, proto, kitename, pport,
                                     secret, backend, lport); }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int pagekite_add_frontend(IntPtr pkm,
            [MarshalAs(UnmanagedType.LPStr)]string domain,
            int port);
        public int add_frontend(string domain, int port) {
            return pagekite_add_frontend(this.pkm, domain, port); }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int pagekite_tick(IntPtr pkm);
        public int tick() { return pagekite_tick(this.pkm); }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int pagekite_poll(IntPtr pkm,
            int timeout);
        public int poll(int timeout) {
            return pagekite_poll(this.pkm, timeout); }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int pagekite_start(IntPtr pkm);
        public int start() { return pagekite_start(this.pkm); }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int pagekite_wait(IntPtr pkm);
        public int wait() { return pagekite_wait(this.pkm); }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int pagekite_stop(IntPtr pkm);
        // FIXME: Need to auto-invoke this in destructor!
        public int stop() { return pagekite_stop(this.pkm); }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int pagekite_get_status(IntPtr pkm);
        public int get_status() { return pagekite_get_status(this.pkm); }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern int pagekite_perror(IntPtr pkm,
            [MarshalAs(UnmanagedType.LPStr)]string prefix);
        public int perror(string prefix) {
            return pagekite_perror(this.pkm, prefix); }

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr pagekite_get_log(IntPtr pkm);
        public string get_log() {
            return Marshal.PtrToStringAnsi(pagekite_get_log(this.pkm)); }
    }
}
