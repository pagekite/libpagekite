using System;
using System.Collections.Generic;
using System.Threading;
using System.Security;

namespace Pagekite
{
    public class PkController
    {
        public PkLib pk;
        public static string APPID = "WinPageKite";
        private volatile bool running;
        private volatile bool exited;
        private string libLog;
//      private Thread logThread;

        public PkController()
        {
            this.libLog = "";
            this.running = false;
            this.exited = false;
            this.pk = new PkLib();
        }

        public bool Start(PkOptions options, Dictionary<string, PkKite> kites)
        {
            int numKites = 4 * (1 + kites.Count);  // 4 possible protocols
            int maxConns = 25;
            int flags = PkLib.PK_WITH_DEFAULTS;
            int verbosity = options.Debug ? 2 : 0;

// FIXME:   if (no network connection)
//          {
//              // Report error?
//              return false;
//          }

            bool ok = true;
            try
            {
                if (!this.pk.init_pagekitenet(APPID,
                                              numKites,
                                              maxConns,
                                              flags,
                                              verbosity))
                {
                    PkLogging.Logger(PkLogging.Level.Error,
                                     "Initializing LibPageKite failed, aborting");
                    return false;
                }
            }
            catch (DllNotFoundException e)
            {
                PkLogging.Logger(PkLogging.Level.Error,
                                 "Could not locate " + PkLib.DLL);
                PkLogging.Logger(PkLogging.Level.Error,
                                 "Message: " + e.Message);
                ok = false;
            }

            if (ok)
            {
                ok = this.StartLogListenerThread();
                if(!ok)
                {
                    this.Stop(false);
                }
            }

            foreach (PkKite kite in kites.Values)
            {
                if (ok && kite.Fly)
                {
                    if (!this.AddKite(kite))
                    {
                        PkLogging.Logger(PkLogging.Level.Error,
                                         "Adding kites failed, aborting");
                        this.Stop(false);
                        ok = false;
                    }
                }
            }

            if (ok)
            {
                int status = this.pk.start();
                if (0 > status)
                {
                    PkLogging.Logger(PkLogging.Level.Error, "Unable to start LibPageKite, aborting");
                    this.Stop(false);
                    ok = false;
                }
            }

            return ok;
        }

        public bool Stop(bool exit)
        {
            bool ok = true;

            if (!this.running)
            {
                return true;
            }

            if (0 > this.pk.stop())
            {
                PkLogging.Logger(PkLogging.Level.Error, "Stopping libpagekite failed");
                ok = false;
            }
            
            if (ok)
            {
                if(exit)
                {
                    this.exited = true;
                }

                this.running = false;
// FIXME:       ok = this.StopLogListenerThread();
            }

            return ok;
        }

        private bool AddKite(PkKite kite)
        {
            bool ok = true;

            if (ok && kite.HTTP.Enabled)
            {
                if (0 > this.pk.add_kite(kite.HTTP.Proto, kite.Domain, 0,
                                         kite.Secret,
                                         "localhost", kite.HTTP.Port))
                {
                    PkLogging.Logger(PkLogging.Level.Error,
                                     "Unable to add kite" +
                                     " | Domain: " + kite.Domain +
                                     " | Proto: " + kite.HTTP.Proto +
                                     " | Port: " + kite.HTTP.Port.ToString());
                    ok = false;
                }
            }

            if (ok && kite.HTTPS.Enabled)
            {
                if (0 > this.pk.add_kite(kite.HTTPS.Proto, kite.Domain, 0,
                                         kite.Secret,
                                         "localhost", kite.HTTPS.Port))
                {
                    PkLogging.Logger(PkLogging.Level.Error,
                                     "Unable to add kite" +
                                     " | Domain: " + kite.Domain +
                                     " | Proto: " + kite.HTTPS.Proto +
                                     " | Port: " + kite.HTTPS.Port.ToString());
                    ok = false;
                }
            }

            if (ok && kite.SSH.Enabled)
            {
                if (0 > this.pk.add_kite(kite.SSH.Proto, kite.Domain, 22,
                                         kite.Secret,
                                         "localhost", kite.SSH.Port))
                {
                    PkLogging.Logger(PkLogging.Level.Error,
                                     "Unable to add kite" +
                                     " | Domain: " + kite.Domain +
                                     " | Proto: " + kite.SSH.Proto +
                                     " | Port: " + kite.SSH.Port.ToString());
                    ok = false;
                }
            }

            if (ok && kite.Minecraft.Enabled)
            {
                if (0 > this.pk.add_kite(kite.Minecraft.Proto, kite.Domain, 0,
                                         kite.Secret,
                                         "localhost", kite.Minecraft.Port))
                {
                    PkLogging.Logger(PkLogging.Level.Error,
                                     "Unable to add kite" +
                                     " | Domain: " + kite.Domain +
                                     " | Proto: " + kite.Minecraft.Proto +
                                     " | Port: " + kite.Minecraft.Port.ToString());
                    ok = false;
                }
            }

            return ok;
        }

        private string GetLog()
        {
            string log;

            try
            {
                log = this.pk.get_log();
            }
            catch(DllNotFoundException e)
            {
                PkLogging.Logger(PkLogging.Level.Error,
                                 "Could not locate libpagekite library");
                PkLogging.Logger(PkLogging.Level.Error,
                                 "Message: " + e.Message);
                log = "Unable to get log";
            }

            if (this.libLog.Equals(log))
            {
                log = null;
            }
            else
            {
                this.libLog = log;
            }

            return log;
        }

        private bool Poll(int timeout)
        {
            if(0 > this.pk.poll(timeout))
            {
                PkLogging.Logger(PkLogging.Level.Error, "Polling failed");
                return false;
            }

            return true;
        }

        private bool StartLogListenerThread()
        {
            bool ok = true;
            this.running = true;
            Thread logThread = new Thread(new ThreadStart(this.LogListen));
            logThread.IsBackground = true;

            try
            {
                logThread.Start();
            }
            catch (Exception e)
            {
                PkLogging.Logger(PkLogging.Level.Error,
                                 "Unable to start logger thread");
                PkLogging.Logger(PkLogging.Level.Error,
                                 "Exception: " + e.Message);
                ok = false;
            }

            return ok;
        }

/* FIXME:
        private bool StopLogListenerThread()
        {
            bool ok = true;

            try
            {
                this.logThread.Abort(); //fixme: this is bad and doesn't work
            }
            catch (Exception e)
            {
                PkLogging.Logger(PkLogging.Level.Error, "Unable to stop logger thread");
                PkLogging.Logger(PkLogging.Level.Error, "Exception: " + e.Message);
                ok = false;
            }

            return ok;
        }
*/
        private void LogListen()
        {
            Thread.Sleep(1200);
            while (this.running)
            {
                if(this.Poll(3600))
                {
                    Thread.Sleep(150);
                    if (!this.exited)
                    {
                        string log = this.GetLog();
                        if (log != null)
                        {
                            PkLogging.Logger(PkLogging.Level.Lib, log);
                        }
                    }
                }
            }
        }
    }
}
