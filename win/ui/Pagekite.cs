using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Windows.Forms;
using System.Threading;

namespace Pagekite
{
    public partial class PkMainForm : Form
    {
        private bool flying;
        private PkController controller;
        private PkData data;

        public PkMainForm()
        {
            PkSplashForm.ShowSplashScreen();

            this.controller = new PkController();
            this.flying = false;
            this.data = PkData.Load();

            this.LogIn();
            this.SynchronizeAccountInfo();

            this.InitForm();
            this.UpdateDaysLeft();
            this.kitePanel.AddKites(this.data.Kites);
            this.StartRenewLoginThread();

            PkLogging.LogPanelUpdate += new EventHandler(this.logPanel.UpdateLog);
            this.logPanel.UpdateLog(this, EventArgs.Empty);

            PkSplashForm.CloseSplashScreen();
        }

        private void LogIn()
        {
            if (this.data.Options.RememberMe)
            {
                PkServiceInfo info = PkService.Login(this.data.ServiceInfo.AccountId, this.data.ServiceInfo.Credentials, true);

                if (info != null)
                {
                    PkLogging.Logger(PkLogging.Level.Info, "Successfully logged in.");
                }
                else
                {
                    PkSplashForm.CloseSplashScreen();
                    this.ShowLoginForm();
                }
            }
            else
            {
                PkSplashForm.CloseSplashScreen();
                this.ShowLoginForm();
            }
        }

        private void ShowLoginForm()
        {
            using (PkLoginForm login = new PkLoginForm())
            {
                var result = login.ShowDialog();
                if (result == DialogResult.OK)
                {
                    this.data.Options.RememberMe = login.RememberMe;
                    this.data.Options.UsePageKiteNet = login.UsePagekiteNet;
                    this.data.ServiceInfo = login.ServiceInfo;
                }
                else
                {
                    Application.Exit();
                }
            }
        }

        private void LogOut()
        {
/*            this.data.Options.RememberMe = false;
            this.data.Save();

            this.Hide();
            this.ShowLoginForm();
            PkSplashForm.ShowSplashScreen();
            this.SynchronizeAccountInfo(); */
        }

        private void SynchronizeAccountInfo()
        {
            PkData serviceData = PkService.GetAccountInfo(this.data.ServiceInfo.AccountId, this.data.ServiceInfo.Credentials);
            Dictionary<string, double> kiteStats = PkService.GetKiteStats(this.data.ServiceInfo.AccountId, this.data.ServiceInfo.Credentials);

            if (serviceData != null && kiteStats != null)
            {
                //Update info about account and kites if it has been manipulated outside of this program

                this.data.ServiceInfo.Secret = serviceData.ServiceInfo.Secret;

                this.data.ServiceInfo.DaysLeft = serviceData.ServiceInfo.DaysLeft;

                this.data.ServiceInfo.MbLeft = serviceData.ServiceInfo.MbLeft;

                List<string> removeDomain = new List<string>();

                //If any kites have been removed, remove them
                foreach (string domain in this.data.Kites.Keys)
                {
                    if (!serviceData.Kites.ContainsKey(domain))
                    {
                        removeDomain.Add(domain);
                    }
                }

                //If any kites are disabled, remove them
                foreach (string domain in kiteStats.Keys)
                {
                    if (kiteStats[domain] < 0)
                    {
                        removeDomain.Add(domain);
                    }
                }

                foreach (string domain in removeDomain)
                {
                    if (this.data.Kites.ContainsKey(domain))
                    {
                        this.data.Kites.Remove(domain);
                    }
                }

                foreach (string domain in serviceData.Kites.Keys)
                {
                    //If new kites have been added, add them, unless they are disabled
                    if (!this.data.Kites.ContainsKey(domain) && kiteStats[domain] > 0)
                    {
                        if (String.IsNullOrEmpty(serviceData.Kites[domain].Secret))
                        {
                            serviceData.Kites[domain].Secret = this.data.ServiceInfo.Secret;
                        }
                        this.data.Kites.Add(domain, serviceData.Kites[domain]);
                    }

                    if (this.data.Kites.ContainsKey(domain))
                    {
                        //Update kite secret if it has changed
                        if (!this.data.Kites[domain].Secret.Equals(serviceData.Kites[domain].Secret)
                            && !String.IsNullOrEmpty(serviceData.Kites[domain].Secret))
                        {
                            this.data.Kites[domain].Secret = serviceData.Kites[domain].Secret;
                        }
                    }
                }

                PkLogging.Logger(PkLogging.Level.Info, "All kites are up to date.");
            }

            if (this.data.Options.RememberMe)
            {
                this.data.Save();
            }
        }

        private void Fly()
        {
            if (this.flying) return;

            if (!this.kitePanel.GetChecked(this.data.Kites))
            {
                PkLogging.Logger(PkLogging.Level.Warning, "No kites selected", true);
                return;
            }

            if (!this.controller.Start(this.data.Options, this.data.Kites))
            {
                PkLogging.Logger(PkLogging.Level.Error, "Unable to fly kites", true);
                return;
            }

            this.flying = true;
            this.flyButton.Enabled = false;
            this.groundButton.Enabled = true;
            this.removeButton.Enabled = false;
            this.addButton.Enabled = false;
            this.kitePanel.UpdateStatus(this.flying, this.data.Kites);
            this.kitePanel.DisableControls();
            this.Notify("Kites are flying");
        }

        private void Ground()
        {
            if (!this.flying) return;

            this.controller.Stop(false);

            this.flying = false;
            this.flyButton.Enabled = true;
            this.groundButton.Enabled = false;
            this.removeButton.Enabled = true;
            this.addButton.Enabled = true;
            this.kitePanel.UpdateStatus(this.flying, this.data.Kites);
            this.kitePanel.EnableControls();
            this.Notify("Kites have been grounded");

            PkLogging.GroundLib();
        }

        private void SaveKite(PkKite kite)
        {
            kite.Secret = this.data.Kites[kite.Domain].Secret;
            this.data.Kites[kite.Domain] = kite;
            this.data.Save();
        }

        private void Notify(string info)
        {
            this.trayIcon.BalloonTipIcon  = ToolTipIcon.Info;
            this.trayIcon.BalloonTipTitle = "Pagekite";
            this.trayIcon.BalloonTipText  = info;

            this.trayIcon.ShowBalloonTip(4000);
        }

        private void UpdateDaysLeft()
        {
            this.daysLeftLabel.Text = "Days left: " + this.data.ServiceInfo.DaysLeft;

            bool ok = true;
            int daysLeft = -1;

            try
            {
                daysLeft = Convert.ToInt32(this.data.ServiceInfo.DaysLeft);
            }
            catch (FormatException e)
            {
                PkLogging.Logger(PkLogging.Level.Error, "days_left is not a number.");
                ok = false;
            }
            catch (OverflowException e)
            {
                PkLogging.Logger(PkLogging.Level.Error, "days_left cannot fit in an Int32.");
                ok = false;
            }

            if (ok && daysLeft < 6)
            {
                this.daysLeftLabel.ForeColor = Color.Red;
                PkLogging.Logger(PkLogging.Level.Warning, 
                                 "You only have " + daysLeft + " days left of your subscription.", true);
            }
        }

        private void ShowWindow()
        {
            this.Show();
            this.WindowState = FormWindowState.Normal;
            this.ShowInTaskbar = true;
        }

        private void StartRenewLoginThread()
        {
            Thread thread = new Thread(new ThreadStart(this.RenewLogin));
            thread.IsBackground = true;

            try
            {
                thread.Start();
            }
            catch (Exception e)
            {
                PkLogging.Logger(PkLogging.Level.Error, "Unable to start RenewLogin thread");
                PkLogging.Logger(PkLogging.Level.Error, "Exception: " + e.Message);
            }
        }

        private void RenewLogin()
        {
            while (true)
            {
                // Renew login every 50 minutes
                Thread.Sleep(50 * 60 * 1000);
                if (!this.IsDisposed)
                {
                    PkService.Login(this.data.ServiceInfo.AccountId, this.data.ServiceInfo.Credentials, true);
                }
            }
        }

        private void OnFly_Click(object sender, EventArgs e)
        {
            this.Fly();
        }

        private void OnFlyMenuItem_Click(object sender, EventArgs e)
        {
            this.Fly();
        }
        
        private void OnGround_Click(object sender, EventArgs e)
        {
            this.Ground();
        }

        private void OnGroundMenuItem_Click(object sender, EventArgs e)
        {
            this.Ground();
        }

        private void OnAdd_Click(object sender, EventArgs e)
        {
            using (PkAddKiteForm addKite = new PkAddKiteForm())
            {
                Dictionary<string, string[]> domains = PkService.GetAvailableDomains(this.data.ServiceInfo.AccountId, this.data.ServiceInfo.Credentials);

                addKite.SetComboBox(domains);

                var result = addKite.ShowDialog();

                if (result == DialogResult.OK)
                {
                    if (PkService.AddKite(this.data.ServiceInfo.AccountId, this.data.ServiceInfo.Credentials, addKite.Kitename))
                    {
                        PkKite kite = new PkKite();
                        kite.Domain = addKite.Kitename;
                        kite.Secret = this.data.ServiceInfo.Secret;
                        this.data.Kites.Add(kite.Domain, kite);
                        this.kitePanel.AddKite(kite);
                        this.data.Save();
                    }
                }
            }
        }

        private void OnRemove_Click(object sender, EventArgs e)
        {
            if (MessageBox.Show("Are you sure you want to delete the selected kites?", "Remove Kite", MessageBoxButtons.YesNo,
                MessageBoxIcon.Question) == DialogResult.Yes)
            {
                if (!this.kitePanel.GetChecked(this.data.Kites))
                {
                    PkLogging.Logger(PkLogging.Level.Warning, "No kites selected", true);
                    return;
                }

                List<string> kitenames = new List<string>();

                foreach (PkKite kite in this.data.Kites.Values)
                {
                    if (kite.Fly)
                    {
                        kitenames.Add(kite.Domain);
                    }
                }

                if (PkService.DeleteKites(this.data.ServiceInfo.AccountId, this.data.ServiceInfo.Credentials, kitenames))
                {
                    foreach (string kite in kitenames)
                    {
                        this.data.Kites.Remove(kite);
                    }

                    this.kitePanel.RemoveKites();
                    this.data.Save();
                }
            }
        }

        private void OnShowDetails_Click(object sender, PkKiteUpdateEventArgs e)
        {
            string key = e.Kite.Domain;

            using (PkKiteDetailsForm detailsForm = new PkKiteDetailsForm())
            {
                detailsForm.setDetails(this.data.Kites[key], this.flying);
                var result = detailsForm.ShowDialog();

                if (result == DialogResult.OK)
                {
                    this.SaveKite(detailsForm.Kite);
                }
            }
        }

        private void OnOptions_Click(object sender, EventArgs e)
        {
            using (PkOptionsForm options = new PkOptionsForm())
            {
                options.SetOptions(this.data.Options);
                var result = options.ShowDialog();

                if(result == DialogResult.OK)
                {
                    this.data.Options.UsePageKiteNet = options.UsePagekiteNet;
                    this.data.Options.Debug          = options.Debug;
                    this.data.Options.RememberMe     = options.RememberMe;

                    this.data.Save();
                }
            }
        }

        private void OnAbout_Click(object sender, EventArgs e)
        {
            using (PkAboutForm about = new PkAboutForm())
            {
                var result = about.ShowDialog();
            }
        }

        private void OnLogOut_Click(object sender, EventArgs e)
        {
            this.LogOut();
        }

        private void OnHelp_Click(object sender, EventArgs e)
        {
            Process.Start("https://pagekite.net/support/");
        }

        private void OnPagekiteNet_Click(object sender, EventArgs e)
        {
            Process.Start("https://pagekite.net/home/");
        }

        private void OnOpen_Click(object sender, EventArgs e)
        {
            this.ShowWindow();
        }

        private void OnTrayIcon_DoubleClick(object sender, MouseEventArgs e)
        {
            this.ShowWindow();
        }

        private void PkMainForm_Resize(object sender, EventArgs e)
        {
            if (this.WindowState == FormWindowState.Minimized)
            {
                this.Hide();
                this.ShowInTaskbar = false;
                if (this.flying)
                {
                    this.Notify("Pagekite is running. Status: Flying.");
                }
                else
                {
                    this.Notify("Pagekite is running. Status: Grounded.");
                }
            }
        }

        private void PkMainForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            if(MessageBox.Show("Are you sure you want to exit?", "Confirm exit", MessageBoxButtons.YesNo, 
               MessageBoxIcon.Question) == DialogResult.Yes)
            {
                this.controller.Stop(true);
            }
            else
            {
                e.Cancel = true;
            }
        }

        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.Run(new PkMainForm());
        }
    }
}
