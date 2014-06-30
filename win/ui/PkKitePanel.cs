using System;
using System.Collections.Generic;
using System.Drawing;
using System.Security;
using System.Windows.Forms;

namespace PageKiteUI
{
    public class PkKitePanel : Panel
    {
        private int numKites;
        private List<PkKite> kites;

        public PkKitePanel()
        {
            this.numKites = 0;
            this.Size = new Size(460, 180);
            this.BackColor = Color.White;
            this.AutoScroll = true;
            this.SetStyle(ControlStyles.UserPaint, true);
        }

        public void AddKite(String name, String domain, String proto, SecureString secret, int port)
        {
            PkKite kite = new PkKite();
            kite.Id = this.numKites++;
            kite.Name = name;
            kite.Domain = domain;
            kite.Proto = proto;
            kite.Secret = secret;
            kite.Port = port;
            kite.Fly = false;
            
            kites.Add(kite);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);

            SolidBrush brush = new SolidBrush(Color.DarkGray);
            Pen pen = new Pen(brush, 2);
            e.Graphics.DrawRectangle(pen, e.ClipRectangle);
        }
    }
}