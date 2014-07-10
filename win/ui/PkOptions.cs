using System;

namespace PageKiteUI
{
    public class PkOptions
    {
        private static class Defaults
        {
            public static readonly bool AllowEviction     = false;
            public static readonly bool BailOnError       = false;
            public static readonly bool DisableUseCurrent = false;
            public static readonly bool FrontendPool      = false;
            public static readonly bool StaticSetup       = false;
            public static readonly bool UseEvil           = false;
            public static readonly bool UseWatchdog       = false;
            public static readonly int AllowEvictionValue  = 30;
            public static readonly int BailOnErrorValue    = 1;
            public static readonly int MaxConnValue        = 25;
            public static readonly int SpareFrontEndsValue = 0;
            public static readonly string FrontendPoolValue = "a DNS name";
        }
/*
        private const bool allowEviction_default     = false;
        private const bool bailOnError_default       = false;
        private const bool disableUseCurrent_default = false;
        private const bool frontendPool_default      = false;
        private const bool staticSetup_default       = false;
        private const bool useEvil_default           = false;
        private const bool useWatchdog_default       = false;
        private const int allowEvictionValue_default  = 30;
        private const int bailOnErrorValue_default    = 1;
        private const int maxConnValue_default        = 25;
        private const int spareFrontendsValue_default = 0;
        private const string frontendPoolValue_default   = "a DNS name";
*/
        public PkOptions()
        {

        }

        public void restoreDefaults()
        {
            this.AllowEviction       = Defaults.AllowEviction;
            this.BailOnError         = Defaults.BailOnError;
            this.DisableUseCurrent   = Defaults.DisableUseCurrent;
            this.FrontendPool        = Defaults.FrontendPool;
            this.StaticSetup         = Defaults.StaticSetup;
            this.UseEvil             = Defaults.UseEvil;
            this.UseWatchdog         = Defaults.UseWatchdog;
            this.AllowEvictionValue  = Defaults.AllowEvictionValue;
            this.BailOnErrorValue    = Defaults.BailOnErrorValue;
            this.MaxConnValue        = Defaults.MaxConnValue;
            this.SpareFrontendsValue = Defaults.SpareFrontEndsValue;
            this.FrontendPoolValue   = Defaults.FrontendPoolValue;
        }

        public bool AllowEviction { get; set; }

        public bool BailOnError { get; set; }

        public bool DisableUseCurrent { get; set; }

        public bool FrontendPool { get; set; }

        public bool StaticSetup { get; set; }

        public bool UseEvil { get; set; }

        public bool UseWatchdog { get; set; }

        public int AllowEvictionValue { get; set; }

        public int BailOnErrorValue { get; set; }

        public int MaxConnValue { get; set; }

        public int SpareFrontendsValue { get; set; }

        public string FrontendPoolValue { get; set; }
    }
}