using System;
using System.Collections.Generic;
using System.Data;
using System.Drawing;
using System.Windows.Forms;
using CookComputing.XmlRpc; //remove

namespace Pagekite
{
    public partial class PkMainForm : Form
    {
//        private Dictionary<string, PkKite> kites;
        private PkController controller;
//        private PkOptions options;
//        private PkServiceInfo serviceInfo;
        private bool flying;
        private PkData data;

        public PkMainForm()
        {
            //show login form

            this.Init();

            this.flying = false;
 //           data.Kites       = new Dictionary<string, PkKite>();
            this.controller  = new PkController();
   //         this.options     = new PkOptions();
 //           this.serviceInfo = new PkServiceInfo();
            this.data = new PkData();

            this.kitePanel.KiteUpdated += new PkKitePanel.PkKiteUpdateHandler(this.HandleKiteUpdate);

            this.data = this.data.Load();

            this.kitePanel.AddKites(this.data.Kites);

            PkLogging.LogPanelUpdate += logPanel.Update;

            //+++++++++++++++++++++++++++++++++++++++++++
    //        IPkService proxy = XmlRpcProxyGen.Create<IPkService>();
    //        string[][] val = proxy.login("sis@pagekite.net", "", "");
    //        MessageBox.Show(val[0][1]);
            //++++++++++++++++++++++++++++++++++++++++++

            // remove ----------------
   /*         PkKite kite = new PkKite();
            kite.Domain = "saevar.pagekite.me";
            kite.Secret = "x4fz49dfa374f7d662eza2z987ckax49";
            kite.HTTP.Port = 80;
            this.AddKite(kite); */
        }

        private void HandleKiteUpdate(object sender, PkKiteUpdateEventArgs e)
        {
            switch(e.Event)
            {
                case "add":
                    this.AddKite(e.Kite);
                    break;
                case "remove":
                    this.RemoveKite(e.Kite);
                    break;
                case "save":
                    this.SaveKite(e.Kite);
                    break;
                case "details":
                    this.OnShowDetails_Click(e.Kite);
                    break;
                default:
                    //Invalid command
                    break;
            }
        }

        private void AddKite(PkKite kite)
        {
            if(this.data.Kites.ContainsKey(kite.Domain))
            {
                return;
            }

            this.data.Kites.Add(kite.Domain, kite);
            kitePanel.AddKite(kite);
        }

        private void RemoveKite(PkKite kite)
        {
            this.data.Kites.Remove(kite.Domain);
        }

        private void SaveKite(PkKite kite)
        {
            this.data.Kites[kite.Domain] = kite;
            this.data.Save();
        }

        private void OnFly_Click(object sender, EventArgs e)
        {
            if (this.flying) return;

            bool kitesSelected = this.kitePanel.GetChecked(this.data.Kites);

            if (!kitesSelected)
            {
                //error, No kites selected
                return;
            }

            bool start = this.controller.Start(this.data.Options, this.data.Kites);

            if (!start)
            {
                //error
                return;
            }

            this.flying = true;
            this.flyButton.Enabled = false;
            this.groundButton.Enabled = true;
            this.kitePanel.UpdateStatus(this.flying, this.data.Kites);
        }

        private void OnGround_Click(object sender, EventArgs e)
        {
            if (!this.flying) return;

            this.controller.Stop();

            this.flying = false;
            this.flyButton.Enabled = true;
            this.groundButton.Enabled = false;
            this.kitePanel.UpdateStatus(this.flying, this.data.Kites);
        }

        private void OnAdd_Click(object sender, EventArgs e)
        {
            PkAddKiteForm addKite = new PkAddKiteForm();
            addKite.SetComboBox(this.data.Kites);
            addKite.ShowDialog();
        }

        private void OnRemove_Click(object sender, EventArgs e)
        {
            //do something more sensible here
            PkLogging.Log(PkLogging.Level.Debug, "whatwhat");

            string ok = this.controller.GetLog();
            this.logPanel.setLog(ok); //remove 
        }

        private void OnShowDetails_Click(PkKite kite)
        {
            string key = kite.Domain;
            PkKiteDetailsForm detailsForm = new PkKiteDetailsForm();
            detailsForm.setDetails(this.data.Kites[key]);
            detailsForm.KiteUpdated += new PkKiteDetailsForm.PkKiteUpdateHandler(this.HandleKiteUpdate);
            detailsForm.ShowDialog();
        }

        private void OnLogOut_Click(object sender, EventArgs e)
        {

        }

        private void OnHelp_Click(object sender, EventArgs e)
        {

        }


        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.Run(new PkMainForm());
        }
    }
}
