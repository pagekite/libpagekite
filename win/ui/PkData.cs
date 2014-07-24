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

        public PkData Load()
        {
            PkData data = new PkData();

            //fixme: catch exepctions
            using (StreamReader file = File.OpenText(@"userconfig.json"))
            {
                JsonSerializer serializer = new JsonSerializer();
                data = (PkData)serializer.Deserialize(file, typeof(PkData));
            }

            return data;
        }

        public void Save()
        {
            //fixme: catch exceptions
            using (StreamWriter file = File.CreateText(@"userconfig.json"))
            {
                JsonSerializer serializer = new JsonSerializer();
                serializer.Serialize(file, this);
            }
        }
    }
}