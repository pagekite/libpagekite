using System;
using System.Security;

namespace PageKiteUI
{
    public class PkKite
    {
        public String Name { get; set; }

        public String Domain { get; set; }

        public String Proto { get; set; }

        public SecureString Secret { get; set; }

        public int Port { get; set; }

        public bool Fly { get; set; }
    }
}