using System;
using System.Collections.Generic;
using System.IO;
using Newtonsoft.Json;

namespace Pagekite
{
    public class PkData
    {
        public PkData()
        {
            this.ServiceInfo = new PkServiceInfo();
            this.Options = new PkOptions();
            this.Kites = new Dictionary<string, PkKite>();
        }

        public PkServiceInfo ServiceInfo { get; set; }

        public PkOptions Options { get; set; }

        public Dictionary<string, PkKite> Kites { get; set; }

        public static PkData Load()
        {
            PkData data = new PkData();

            string dir = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
            dir = Path.Combine(dir, "PageKite");
            string filepath = Path.Combine(dir, "userconfig.json");

            if(File.Exists(filepath))
            {
                try
                {
                    using (StreamReader file = File.OpenText(@filepath))
                    {
                        JsonSerializer serializer = new JsonSerializer();
                        data = (PkData)serializer.Deserialize(file, typeof(PkData));
                    }
                }
                catch(Exception e)
                {
                    PkLogging.Logger(PkLogging.Level.Error, "Unable to load settings.", true);
                    PkLogging.Logger(PkLogging.Level.Error, "Exception: " + e.Message);
                    data = new PkData();
                }
            }
            else
            {
                data = new PkData();
            }

            if(data == null)
            {
                data = new PkData();
            }

            return data;
        }

        public void Save()
        {
            //Save settings to %AppData% where we should always have read/write access

            string dir = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
            dir = Path.Combine(dir, "PageKite");

            if(!Directory.Exists(dir))
            {
                try
                {
                    Directory.CreateDirectory(dir);
                }
                catch(Exception e)
                {
                    PkLogging.Logger(PkLogging.Level.Error, "Unable to save settings.", true);
                    PkLogging.Logger(PkLogging.Level.Error, "Exception: " + e.Message);
                }
            }

            string filepath = Path.Combine(dir, "userconfig.json");

            try
            {
                using (StreamWriter file = File.CreateText(@filepath))
                {
                    JsonSerializer serializer = new JsonSerializer();
                    serializer.Serialize(file, this);
                }
            }
            catch(Exception e)
            {
                PkLogging.Logger(PkLogging.Level.Error, "Unable to save settings.", true);
                PkLogging.Logger(PkLogging.Level.Error, "Exception: " + e.Message);
            }
        }
    }
}