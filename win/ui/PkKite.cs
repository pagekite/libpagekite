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
        }

        public string Domain { get; set; }

        public string Secret { get; set; }

        public bool Fly { get; set; }

/*        public PkTransport HTTP { get; set; }

        public PkTransport HTTPS { get; set; }

        public PkTransport SSH { get; set; }

        public PkTransport Minecraft { get; set; }

/*        public int HTTPPort { get; set; }

        public int HTTPSPort { get; set; }

        public int SSHPort { get; set; }

        public int MinecraftPort { get; set; }

        public string HTTPProto
        {
            get
            {
                return "HTTP";
            }
        }

        public string HTTPSProto
        {
            get
            {
                return "HTTPS";
            }
        }

        public string SSHProto
        {
            get
            {
                return "SSH";
            }
        }

        public string MinecraftProto
        {
            get
            {
                return "Minecraft"; //fixme: ??
            }
        } */
    }
}