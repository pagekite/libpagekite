using System;
using System.Collections.Generic;
using System.Data;
using System.Drawing;
using System.Windows.Forms;

namespace PageKiteUI
{
    public class PageKite : PkMainForm
    {
        private Dictionary<string, PkKite> kites;
        private PkController controller;
        private PkOptions options;

        public PageKite()
        {

        }


        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.Run(new PageKite());
        }
    }
}
