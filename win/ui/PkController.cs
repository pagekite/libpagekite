using System;
using System.Collections.Generic;

namespace Pagekite
{
    public class PkController
    {
        public PkController()
        {

        }

        public bool Start(PkOptions options, Dictionary<string, PkKite> kites)
        {
     /*       if (no network connection)
            {
                //error
                return false;
            }*/

            int numKites = kites.Count;
            int maxKites = 25;
            int staticSetup = 0;
            int spareFrontends = 0;
            int verbosity = 2; // fix

            if (0 > PkImport.libpagekite_init(numKites, maxKites, staticSetup, spareFrontends, verbosity))
            {
                //error
                return false;
            }

            foreach (PkKite kite in kites.Values)
            {
                if (kite.Fly)
                {
                    bool add = this.AddKite(kite);

                    if (!add)
                    {
                        //error
                        this.Stop();
                        return false;
                    }
                }
            }

            if(0 > PkImport.libpagekite_start())
            {
                //error
                return false;
            }

            return true;
        }

        public bool AddKite(PkKite kite)
        {
            if (kite.HTTP.Enabled)
            {
                if (0 > PkImport.libpagekite_addKite(kite.HTTP.Proto, kite.Domain, 0, kite.Secret, "localhost", kite.HTTP.Port))
                {
                    //error
                    //return false;
                }
            }

            if (kite.HTTPS.Enabled)
            {
                if (0 > PkImport.libpagekite_addKite(kite.HTTPS.Proto, kite.Domain, 0, kite.Secret, "localhost", kite.HTTPS.Port))
                {
                    //error
                    //return false;
                }
            }

            if (kite.SSH.Enabled)
            {
                if (0 > PkImport.libpagekite_addKite(kite.SSH.Proto, kite.Domain, 22, kite.Secret, "localhost", kite.SSH.Port)) // fixme: 22?
                {
                    //error
                    //return false;
                }
            }

            if (kite.Minecraft.Enabled)
            {
                if (0 > PkImport.libpagekite_addKite(kite.Minecraft.Proto, kite.Domain, 0, kite.Secret, "localhost", kite.Minecraft.Port))
                {
                    //error
                    //return false;
                }
            }

            return true;
        }

        public void Stop()
        {
            if(0 > PkImport.libpagekite_stop())
            {
                //error
            }
        }

        public string GetLog()
        {
            string log = "[lib] " + System.Runtime.InteropServices.Marshal.PtrToStringAnsi(PkImport.libpagekite_getLog());
            log = log.Replace("\n", Environment.NewLine + "[lib] ");
            return log.Substring(0, log.Length - 6);
        }
    }
}