using System;
using System.Diagnostics;
using System.Drawing;
using System.Windows.Forms;
using CookComputing.XmlRpc;

namespace Pagekite
{
    public class PkLoginForm : Form
    {
        public bool RememberMe { get; set; }
        public bool UsePagekiteNet { get; set; }
        public PkServiceInfo ServiceInfo { get; set; }

        private WelcomePanel welcomePanel;
        private LoginPanel loginPanel;

        public PkLoginForm()
        {
            this.welcomePanel = new WelcomePanel();
            this.loginPanel = new LoginPanel();
            this.loginPanel.Hide();

            this.Controls.Add(this.welcomePanel);
            this.Controls.Add(this.loginPanel);

            this.ClientSize      = new Size(380, 380);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox     = false;
            this.ShowInTaskbar   = false;
            this.StartPosition   = FormStartPosition.CenterScreen;
            this.Text            = "PageKite - Log in";
        }

        public void ShowLogin()
        {
            this.loginPanel.Show();
        }

        public void Close_Login()
        {
            this.UsePagekiteNet = welcomePanel.UsePagekiteNet;
            this.RememberMe = loginPanel.RememberMe;
            this.ServiceInfo = loginPanel.ServiceInfo;
            this.DialogResult = DialogResult.OK;
            this.Close();
        }

        private class LoginPanel : Panel
        {
            public PkServiceInfo ServiceInfo { get; set; }
            public bool RememberMe;

            private Button confirmButton;
            private Button cancelButton;
            private CheckBox agreeCheckBox;
            private CheckBox rememberCheckBox;
            private LinkLabel termsLinkLabel;
            private TextBox emailTextBox;
            private TextBox kiteTextBox;
            private TextBox passwordTextBox;

            public LoginPanel()
            {
                this.confirmButton    = new Button();
                this.cancelButton     = new Button();
                this.agreeCheckBox    = new CheckBox();
                this.rememberCheckBox = new CheckBox();

                Font font = new Font("Tahoma", 10);
                Padding labelMargin = new Padding(2, 12, 2, 12);
                Padding textBoxMargin = new Padding(0, 10, 0, 11);

                FlowLayoutPanel labelPanel    = new FlowLayoutPanel();
                FlowLayoutPanel textBoxPanel  = new FlowLayoutPanel();
                FlowLayoutPanel kiteNamePanel = new FlowLayoutPanel();
                FlowLayoutPanel termsPanel    = new FlowLayoutPanel();
                FlowLayoutPanel buttonPanel   = new FlowLayoutPanel();
                Panel labelSpacing   = new Panel();
                Panel textBoxSpacing = new Panel();

                Label emailLabel = this.MakeLabel(font, labelMargin, "E-Mail: ");
                Label kiteLabel = this.MakeLabel(font, labelMargin, "Kite name: ");
                Label isPasswordLabel = this.MakeLabel(font, labelMargin, "If you already have an account: ");
                Label passwordLabel = this.MakeLabel(font, labelMargin, "Password: ");
                Label kitePathLabel = this.MakeLabel(font, labelMargin, ".pagekite.me");

                this.termsLinkLabel = new LinkLabel();

                this.emailTextBox    = this.MakeTextBox(160, font, textBoxMargin);
                this.kiteTextBox     = this.MakeTextBox(120, font, textBoxMargin);
                this.passwordTextBox = this.MakeTextBox(160, font, textBoxMargin);

                this.passwordTextBox.PasswordChar = '*';

                labelPanel.Location      = new Point(20, 40);
                labelPanel.Size          = new Size(100, 280);
                labelPanel.FlowDirection = FlowDirection.TopDown;

                textBoxPanel.Location      = new Point(120, 40);
                textBoxPanel.Size          = new Size(220, 280);
                textBoxPanel.FlowDirection = FlowDirection.TopDown;

                kiteNamePanel.Size          = new Size(220, 40);
                kiteNamePanel.Margin        = new Padding(0);
                kiteNamePanel.FlowDirection = FlowDirection.LeftToRight;

                termsPanel.Size          = new Size(220, 40);
                termsPanel.Margin        = new Padding(0);
                termsPanel.FlowDirection = FlowDirection.LeftToRight;

                buttonPanel.Location      = new Point(95, 320);
                buttonPanel.Size          = new Size(200, 50);
                buttonPanel.FlowDirection = FlowDirection.LeftToRight;

                labelSpacing.Size   = new Size(20, 90);
                textBoxSpacing.Size = new Size(20, 53);

                isPasswordLabel.Location = new Point(20, 180);
                isPasswordLabel.Margin   = new Padding(0);

                this.termsLinkLabel.AutoSize = true;
                this.termsLinkLabel.Margin = new Padding(0, 10, 0, 0);
                this.termsLinkLabel.VisitedLinkColor = Color.Navy;
                this.termsLinkLabel.LinkColor = Color.Blue;
                this.termsLinkLabel.LinkBehavior = LinkBehavior.HoverUnderline;
                this.termsLinkLabel.Font = font;
                this.termsLinkLabel.Text = "I accept the Terms of Service";
                this.termsLinkLabel.Links.Add(13, 16, "https://pagekite.net/support/terms/");
                this.termsLinkLabel.LinkClicked += new LinkLabelLinkClickedEventHandler(this.OnTermsLinkLabel_Click);

                this.rememberCheckBox.Width   = 210;
                this.rememberCheckBox.Margin  = new Padding(0, 10, 0, 0);
                this.rememberCheckBox.Font    = font;
                this.rememberCheckBox.Checked = true;
                this.rememberCheckBox.Text    = "Remember me";

                this.agreeCheckBox.AutoSize = true;
                this.agreeCheckBox.Margin = new Padding(0, 11, 0, 0);
                this.agreeCheckBox.Font   = font;
                this.agreeCheckBox.Text   = "";

                this.confirmButton.Size  = new Size(90, 30);
                this.confirmButton.Font  = font;
                this.confirmButton.Text  = "Confirm";
                this.confirmButton.Click += new EventHandler(this.OnOk_Click);

                this.cancelButton.Size  = new Size(90, 30);
                this.cancelButton.Font  = font;
                this.cancelButton.Text  = "Cancel";
                this.cancelButton.Click += new EventHandler(this.OnCancel_Click);

                kiteNamePanel.Controls.Add(this.kiteTextBox);
                kiteNamePanel.Controls.Add(kitePathLabel);

                termsPanel.Controls.Add(this.agreeCheckBox);
                termsPanel.Controls.Add(this.termsLinkLabel);

                labelPanel.Controls.Add(emailLabel);
                labelPanel.Controls.Add(kiteLabel);
                labelPanel.Controls.Add(labelSpacing);
                labelPanel.Controls.Add(passwordLabel);

                textBoxPanel.Controls.Add(this.emailTextBox);
                textBoxPanel.Controls.Add(kiteNamePanel);
                textBoxPanel.Controls.Add(termsPanel);
                textBoxPanel.Controls.Add(textBoxSpacing);
                textBoxPanel.Controls.Add(this.passwordTextBox);
                textBoxPanel.Controls.Add(this.rememberCheckBox);

                buttonPanel.Controls.Add(this.confirmButton);
                buttonPanel.Controls.Add(this.cancelButton);

                this.Controls.Add(isPasswordLabel);
                this.Controls.Add(labelPanel);
                this.Controls.Add(textBoxPanel);
                this.Controls.Add(buttonPanel);

                this.Size = new Size(380, 380);
            }

            private Label MakeLabel(Font font, Padding padding, string text)
            {
                Label label = new Label();

                label.AutoSize = true;
                label.Font = font;
                label.Padding = padding;
                label.Text = text;

                return label;
            }

            private TextBox MakeTextBox(int width, Font font, Padding margin)
            {
                TextBox textBox = new TextBox();

                textBox.Width = width;
                textBox.Font = font;
                textBox.Margin = margin;

                return textBox;
            }

            private bool ValidateEmail(string email)
            {
                return (email.Contains("@") && email.Contains("."));
            }

            private void OnTermsLinkLabel_Click(object sender, LinkLabelLinkClickedEventArgs e)
            {
                this.termsLinkLabel.Links[this.termsLinkLabel.Links.IndexOf(e.Link)].Visited = true;
                Process.Start(e.Link.LinkData as string);
            }

            private void OnOk_Click(object sender, EventArgs e)
            {
                string email = this.emailTextBox.Text;
                string kiteName = this.kiteTextBox.Text;
                string password = this.passwordTextBox.Text;

                if (!this.ValidateEmail(email))
                {
                    PkLogging.Logger(PkLogging.Level.Warning, "Invalid email address", true);
                    return;
                }
                if (string.IsNullOrEmpty(kiteName))
                {
                    PkLogging.Logger(PkLogging.Level.Warning, "Invalid kite name", true);
                    return;
                }
                
                if (string.IsNullOrEmpty(password))
                {
                    if (this.agreeCheckBox.Checked)
                    {
                        if (PkService.CreateAccount(email, kiteName))
                        {
                            PkLogging.Logger(PkLogging.Level.Info, "An email containing your password and details has been sent to " + email, true);
                            return;
                        }
                        else
                        {
                            return;
                        }
                    }
                    else
                    {
                        PkLogging.Logger(PkLogging.Level.Warning, "You must agree to the terms of service to continue.", true);
                    }
                }
                else
                {
                    PkServiceInfo info = PkService.Login(email, password, false);

                    if (info != null)
                    {
                        PkLogging.Logger(PkLogging.Level.Info, "Successfully signed in.");

                        password = "";
                        this.passwordTextBox.Text = "";

                        this.RememberMe = this.rememberCheckBox.Checked;
                        this.ServiceInfo = info;

                        PkLoginForm parent = (this.Parent as PkLoginForm);
                        parent.Close_Login();
                    }
                    else
                    {
                        return;
                    }
                }
            }

            private void OnCancel_Click(object sender, EventArgs e)
            {
            
            }
        }

        private class WelcomePanel : Panel
        {
            private Button helpButton;
            private Button notNowButton;
            private Button yesButton;

            public bool UsePagekiteNet { get; set; }

            public WelcomePanel()
            {
                this.helpButton = new Button();
                this.notNowButton = new Button();
                this.yesButton = new Button();

                PictureBox pictureBox = new PictureBox();
                Image logo;

                try
                {
                    logo = Image.FromFile("img\\pagekite-logo-letters.png");
                }
                catch(Exception e)
                {
                    PkLogging.Logger(PkLogging.Level.Error, "Could not locate pagekite logo");
                    PkLogging.Logger(PkLogging.Level.Error, "Exception: " + e.Message);
                    logo = null;
                }

                Label welcomeLabel = new Label();
                Label usePageKiteNetLabel = new Label();

                FlowLayoutPanel buttonPanel = new FlowLayoutPanel();
                Panel welcomePanel = new Panel();
                Panel usePageKiteNetPanel = new Panel();

                Font font = new Font("Tahoma", 10);

                this.Size = new Size(380, 380);

                if (logo != null)
                {
                    pictureBox.Location = new Point(this.Width/2 - logo.Width/2, 20);
                    pictureBox.Image = logo;
                    pictureBox.Height = logo.Height;
                    pictureBox.Width = logo.Width;
                }

                welcomePanel.Location = new Point(0, 210);
                welcomePanel.Size = new Size(this.Width, 40);

                usePageKiteNetPanel.Location = new Point(0, 270);
                usePageKiteNetPanel.Size = new Size(this.Width, 30);

                welcomeLabel.AutoSize = false;
                welcomeLabel.TextAlign = ContentAlignment.MiddleCenter;
                welcomeLabel.Dock = DockStyle.Fill;
                welcomeLabel.Font = new Font("Tahoma", 14);
                welcomeLabel.Text = "Welcome to PageKite.";

                usePageKiteNetLabel.AutoSize = false;
                usePageKiteNetLabel.TextAlign = ContentAlignment.MiddleCenter;
                usePageKiteNetLabel.Dock = DockStyle.Fill;
                usePageKiteNetLabel.Font = font;
                usePageKiteNetLabel.Text = "Do you want to use pagekite.net as your relay?";

                buttonPanel.Location = new Point(45, 320);
                buttonPanel.Size = new Size(300, 50);
                buttonPanel.FlowDirection = FlowDirection.LeftToRight;

                this.helpButton.Size = new Size(90, 30);
                this.helpButton.Font = font;
                this.helpButton.Text = "Help?";
                this.helpButton.Click += new EventHandler(this.OnHelp_Click);

                this.yesButton.Size = new Size(90, 30);
                this.yesButton.Font = font;
                this.yesButton.Text = "Yes";
                this.yesButton.Click += new EventHandler(this.OnYes_Click);

                this.notNowButton.Size = new Size(90, 30);
                this.notNowButton.Font = font;
                this.notNowButton.Text = "Not now";
                this.notNowButton.Click += new EventHandler(this.OnNotNow_Click);

                buttonPanel.Controls.Add(this.yesButton);
                buttonPanel.Controls.Add(this.notNowButton);
                buttonPanel.Controls.Add(this.helpButton);

                welcomePanel.Controls.Add(welcomeLabel);
                usePageKiteNetPanel.Controls.Add(usePageKiteNetLabel);

                this.Controls.Add(pictureBox);
                this.Controls.Add(welcomePanel);
                this.Controls.Add(usePageKiteNetPanel);
                this.Controls.Add(buttonPanel);
            }

            private void OnYes_Click(object sender, EventArgs e)
            {
                this.Hide();
                this.UsePagekiteNet = true;
                PkLoginForm parent = (this.Parent as PkLoginForm);
                parent.ShowLogin();
            }

            private void OnNotNow_Click(object sender, EventArgs e)
            {
                this.Hide();
                this.UsePagekiteNet = false;
                PkLoginForm parent = (this.Parent as PkLoginForm);
                parent.ShowLogin();
            }

            private void OnHelp_Click(object sender, EventArgs e)
            {
                Process.Start("https://pagekite.net/support/");
            }
        }
    }
}