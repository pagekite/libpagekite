using System;
using System.Collections.Generic;
using System.Threading;
using System.Security;

namespace Pagekite
{
    public class PkController
    {
        private volatile bool running;
        private volatile bool exited;
        private string libLog;
     //   private Thread logThread;

        public PkController()
        {
            this.libLog = "";
            this.running = false;
            this.exited = false;
        }

        public bool Start(PkOptions options, Dictionary<string, PkKite> kites)
        {
     /*       if (no network connection)
            {
                //error
                return false;
            }*/
            bool ok = true;

            int numKites = kites.Count;
            int maxKites = 25;
            int staticSetup = 0;
            int spareFrontends = 0;
            int verbosity = options.Debug ? 2 : 0;

            try
            {
                int error = PkImport.libpagekite_init(numKites, maxKites, staticSetup, spareFrontends, verbosity);

                if (error != 0)
                {
                    PkLogging.Logger(PkLogging.Level.Error, "Initializing LibPageKite failed, aborting");
                    PkLogging.Logger(PkLogging.Level.Error, "Error: " + error.ToString());
                    return false;
                }

                /*
                if (0 > PkImport.libpagekite_init(numKites, maxKites, staticSetup, spareFrontends, verbosity))
                {
                    PkLogging.Logger(PkLogging.Level.Error, "Initializing LibPageKite failed, aborting");
                    return false;
                }*/
            }
            catch (DllNotFoundException e)
            {
                PkLogging.Logger(PkLogging.Level.Error, "Could not locate libpagekite library");
                PkLogging.Logger(PkLogging.Level.Error, "Message: " + e.Message);
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
                    bool add = this.AddKite(kite);

                    if (!add)
                    {
                        PkLogging.Logger(PkLogging.Level.Error, "Adding kites failed, aborting");
                        this.Stop(false);
                        ok = false;
                    }
                }
            }

            if(ok)
            {
                if (0 > PkImport.libpagekite_start())
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

            if (0 > PkImport.libpagekite_stop())
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
     ///           ok = this.StopLogListenerThread();
            }

            return ok;
        }

        private bool AddKite(PkKite kite)
        {
            bool ok = true;

            if (ok && kite.HTTP.Enabled)
            {
                if (0 > PkImport.libpagekite_addKite(kite.HTTP.Proto, kite.Domain, 0, kite.Secret, "localhost", kite.HTTP.Port))
                {
                    PkLogging.Logger(PkLogging.Level.Error, "Unable to add kite | Domain: " + kite.Domain 
                        + " | Proto: " + kite.HTTP.Proto + " | Port: " + kite.HTTP.Port.ToString());
                    ok = false;
                }
            }

            if (ok && kite.HTTPS.Enabled)
            {
                if (0 > PkImport.libpagekite_addKite(kite.HTTPS.Proto, kite.Domain, 0, kite.Secret, "localhost", kite.HTTPS.Port))
                {
                    PkLogging.Logger(PkLogging.Level.Error, "Unable to add kite | Domain: " + kite.Domain
                        + " | Proto: " + kite.HTTPS.Proto + " | Port: " + kite.HTTPS.Port.ToString());
                    ok = false;
                }
            }

            if (ok && kite.SSH.Enabled)
            {
                if (0 > PkImport.libpagekite_addKite(kite.SSH.Proto, kite.Domain, 22, kite.Secret, "localhost", kite.SSH.Port))
                {
                    PkLogging.Logger(PkLogging.Level.Error, "Unable to add kite | Domain: " + kite.Domain
                        + " | Proto: " + kite.Minecraft.Proto + " | Port: " + kite.SSH.Port.ToString());
                    ok = false;
                }
            }

            if (ok && kite.Minecraft.Enabled)
            {
                if (0 > PkImport.libpagekite_addKite(kite.Minecraft.Proto, kite.Domain, 0, kite.Secret, "localhost", kite.Minecraft.Port))
                {
                    PkLogging.Logger(PkLogging.Level.Error, "Unable to add kite | Domain: " + kite.Domain
                        + " | Proto: " + kite.Minecraft.Proto + " | Port: " + kite.Minecraft.Port.ToString());
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
                log = System.Runtime.InteropServices.Marshal.PtrToStringAnsi(PkImport.libpagekite_getLog());
            }
            catch(DllNotFoundException e)
            {
                PkLogging.Logger(PkLogging.Level.Error, "Could not locate libpagekite library");
                PkLogging.Logger(PkLogging.Level.Error, "Message: " + e.Message);
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
            if(0 > PkImport.libpagekite_poll(timeout))
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
                PkLogging.Logger(PkLogging.Level.Error, "Unable to start logger thread");
                PkLogging.Logger(PkLogging.Level.Error, "Exception: " + e.Message);
                ok = false;
            }

            return ok;
        }

/*        private bool StopLogListenerThread()
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