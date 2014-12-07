using System;
using System.Security;

namespace Pagekite
{
    public class PkKite
    {
        public PkTransport HTTP;
        public PkTransport HTTPS;
        public PkTransport SSH;
        public PkTransport Minecraft;

        public PkKite()
        {
            this.HTTP      = new PkTransport();
            this.HTTPS     = new PkTransport();
            this.SSH       = new PkTransport();
            this.Minecraft = new PkTransport();

            this.HTTP.Proto      = "http";
            this.HTTPS.Proto     = "https";
            this.SSH.Proto       = "ssh";
            this.Minecraft.Proto = "minecraft";

            this.HTTP.Port = 80;
            this.HTTPS.Port = 443;
            this.SSH.Port = 22;
            this.Minecraft.Port = 25565;
        }

        public string Domain { get; set; }

        public string Secret { get; set; }

        public bool Fly { get; set; }

    }
}