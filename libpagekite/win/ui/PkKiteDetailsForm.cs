using System;
using System.Collections.Generic;
using System.Drawing;
using System.Security;
using System.Windows.Forms;

namespace Pagekite
{
    public class PkKiteDetailsForm : Form
    {
/*
        public delegate void PkKiteUpdateHandler(
            object sender, PkKiteUpdateEventArgs e);

        public event PkKiteUpdateHandler KiteUpdated;
  */
        public PkKite Kite;

        private Button   saveButton;
        private Button   cancelButton;
        private CheckBox httpCheckBox;
        private CheckBox httpsCheckBox;
        private CheckBox sshCheckBox;
        private CheckBox minecraftCheckBox;
        private Label    domainLabel;
//        private TextBox  domainTextBox;
        private TextBox  httpPortTextBox;
        private TextBox  httpsPortTextBox;
        private TextBox  sshPortTextBox;
        private TextBox  minecraftPortTextBox;

        public PkKiteDetailsForm()
        {
            this.saveButton    = new Button();
            this.cancelButton  = new Button();
            this.domainLabel   = new Label();

            FlowLayoutPanel labelFlowPanel    = new FlowLayoutPanel();
            FlowLayoutPanel settingsFlowPanel = new FlowLayoutPanel();
            
            Panel domainPanel = new Panel();

            Font font = new Font("Tahoma", 10);
            Padding checkBoxMargin  = new Padding(2, 24, 2, 24);
            Padding textBoxMargin   = new Padding(0, 22, 0, 22);

//            this.domainTextBox        = this.MakeTextBox(200, font, textBoxMargin);
            this.httpPortTextBox      = this.MakeTextBox(80, font, textBoxMargin);
            this.httpsPortTextBox     = this.MakeTextBox(80, font, textBoxMargin);
            this.sshPortTextBox       = this.MakeTextBox(80, font, textBoxMargin);
            this.minecraftPortTextBox = this.MakeTextBox(80, font, textBoxMargin);

            this.httpCheckBox      = this.MakeCheckBox(font, checkBoxMargin, " HTTP");
            this.httpsCheckBox     = this.MakeCheckBox(font, checkBoxMargin, " HTTPS");
            this.sshCheckBox       = this.MakeCheckBox(font, checkBoxMargin, " SSH");
            this.minecraftCheckBox = this.MakeCheckBox(font, checkBoxMargin, " Minecraft");

            Label httpLabel      = new Label();
            Label httpsLabel     = new Label();
            Label sshLabel       = new Label();
            Label minecraftLabel = new Label();

/*            Label protoLabel  = this.MakeLabel(font, checkBoxMargin, "Protocol:");
            Label portLabel   = this.MakeLabel(font, checkBoxMargin, "Local port:"); */
   //         Label protoLabel  = this.MakeLabel(font, labelPadding, "Protocol:");
   //        Label portLabel   = this.MakeLabel(font, labelPadding, "Port:");

            labelFlowPanel.Location      = new Point(65, 60);
            labelFlowPanel.Size          = new Size(140, 300);
            labelFlowPanel.FlowDirection = FlowDirection.TopDown;

            settingsFlowPanel.Location      = new Point(210, 60);
            settingsFlowPanel.Size          = new Size(200, 300);
            settingsFlowPanel.FlowDirection = FlowDirection.TopDown;

            domainPanel.Location = new Point(0, 0);
            domainPanel.Size     = new Size(360, 80);

            httpLabel.Location = new Point(65, 110);
            httpLabel.Font     = font;
            httpLabel.AutoSize = true;
            httpLabel.Text     = "The local port of your HTTP server";

            httpsLabel.Location = new Point(65, 178);
            httpsLabel.Font     = font;
            httpsLabel.AutoSize = true;
            httpsLabel.Text     = "The local port of your HTTPS server";

            sshLabel.Location = new Point(65, 245);
            sshLabel.Font     = font;
            sshLabel.AutoSize = true;
            sshLabel.Text     = "The local port of your SSH server";

            minecraftLabel.Location = new Point(65, 315);
            minecraftLabel.Font = font;
            minecraftLabel.AutoSize = true;
            minecraftLabel.Text = "The local port of your Minecraft server";

            this.domainLabel.Font      = new Font("Tahoma", 13);
            this.domainLabel.AutoSize  = false;
            this.domainLabel.TextAlign = ContentAlignment.MiddleCenter;
            this.domainLabel.Dock      = DockStyle.Fill;

            this.httpPortTextBox.Enabled   = false;
            this.httpPortTextBox.MaxLength = 5;
            this.httpPortTextBox.KeyPress += new KeyPressEventHandler(this.textBox_KeyPress);

            this.httpsPortTextBox.Enabled   = false;
            this.httpsPortTextBox.MaxLength = 5;
            this.httpsPortTextBox.KeyPress += new KeyPressEventHandler(this.textBox_KeyPress);

            this.sshPortTextBox.Enabled   = false;
            this.sshPortTextBox.MaxLength = 5;
            this.sshPortTextBox.KeyPress += new KeyPressEventHandler(this.textBox_KeyPress);

            this.minecraftPortTextBox.Enabled   = false;
            this.minecraftPortTextBox.MaxLength = 5;
            this.minecraftPortTextBox.KeyPress += new KeyPressEventHandler(this.textBox_KeyPress);
            
            this.httpCheckBox.Click      += new EventHandler(this.OnHttp_Click);
            this.httpsCheckBox.Click     += new EventHandler(this.OnHttps_Click);
            this.sshCheckBox.Click       += new EventHandler(this.OnSsh_Click);
            this.minecraftCheckBox.Click += new EventHandler(this.OnMinecraft_Click);
            
            this.saveButton.Location = new Point(70, 370);
            this.saveButton.Size     = new Size(90, 30);
            this.saveButton.Font     = font;
            this.saveButton.Text     = "Save";
            this.saveButton.Click   += new EventHandler(this.OnSave_Click);

            this.cancelButton.Location = new Point(190, 370);
            this.cancelButton.Size     = new Size(90, 30);
            this.cancelButton.Font     = font;
            this.cancelButton.Text     = "Cancel";
            this.cancelButton.Click   += new EventHandler(this.OnCancel_Click);

            domainPanel.Controls.Add(this.domainLabel);

//            labelFlowPanel.Controls.Add(this.domainLabel);
  //          labelFlowPanel.Controls.Add(protoLabel);
  /*          labelFlowPanel.Controls.Add(portLabel); */
            labelFlowPanel.Controls.Add(this.httpCheckBox);
            labelFlowPanel.Controls.Add(this.httpsCheckBox);
            labelFlowPanel.Controls.Add(this.sshCheckBox);
            labelFlowPanel.Controls.Add(this.minecraftCheckBox);

//            settingsFlowPanel.Controls.Add(this.domainTextBox);
 //           settingsFlowPanel.Controls.Add(portLabel);
//            settingsFlowPanel.Controls.Add(this.protoComboBox);
            settingsFlowPanel.Controls.Add(this.httpPortTextBox);
            settingsFlowPanel.Controls.Add(this.httpsPortTextBox);
            settingsFlowPanel.Controls.Add(this.sshPortTextBox);
            settingsFlowPanel.Controls.Add(this.minecraftPortTextBox);

            this.Controls.Add(httpLabel);
            this.Controls.Add(httpsLabel);
            this.Controls.Add(sshLabel);
            this.Controls.Add(minecraftLabel);
            this.Controls.Add(domainPanel);
            this.Controls.Add(labelFlowPanel);
            this.Controls.Add(settingsFlowPanel);
            this.Controls.Add(saveButton);
            this.Controls.Add(cancelButton);

            this.ClientSize      = new Size(360, 420);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox     = false;
            this.StartPosition   = FormStartPosition.CenterScreen;
            this.Text            = "PageKite - Kite details";
        }

        public void setDetails(PkKite kite, bool flying)
        {
            if(flying)
            {
                this.saveButton.Enabled = false;
            }

            this.domainLabel.Text = kite.Domain;

            this.httpCheckBox.Checked = kite.HTTP.Enabled;
            this.httpsCheckBox.Checked = kite.HTTPS.Enabled;
            this.sshCheckBox.Checked = kite.SSH.Enabled;
            this.minecraftCheckBox.Checked = kite.Minecraft.Enabled;

            this.httpPortTextBox.Enabled = kite.HTTP.Enabled;
            this.httpsPortTextBox.Enabled = kite.HTTPS.Enabled;
            this.sshPortTextBox.Enabled = kite.SSH.Enabled;
            this.minecraftPortTextBox.Enabled = kite.Minecraft.Enabled;

            this.httpPortTextBox.Text = kite.HTTP.Port.ToString();
            this.httpsPortTextBox.Text = kite.HTTPS.Port.ToString();
            this.sshPortTextBox.Text = kite.SSH.Port.ToString();
            this.minecraftPortTextBox.Text = kite.Minecraft.Port.ToString();
        }

        private Label MakeLabel(Font font, Padding padding, string text)
        {
            Label label = new Label();   
         
            label.AutoSize = true;
            label.Font     = font;
            label.Padding  = padding;
            label.Text     = text;

            return label;
        }

        private CheckBox MakeCheckBox(Font font, Padding margin, string text)
        {
            CheckBox checkBox = new CheckBox();

            checkBox.AutoSize = true;
            checkBox.Font     = font;
            checkBox.Margin   = margin;
            checkBox.Text     = text;

            return checkBox;
        }

        private TextBox MakeTextBox(int width, Font font, Padding margin)
        {
            TextBox textBox = new TextBox();

            textBox.Width  = width;
            textBox.Font   = font;
            textBox.Margin = margin;

            return textBox;
        }

        private int validatePort(string sPort)
        {
            int port = 0; 

            if (!Int32.TryParse(sPort, out port))
            {
                //invalid port number
                return 0;
            }

            if(0 > port || port > 65535)
            {
                //invalid port number
                return 0;
            }

            return port;
        }

        private void textBox_KeyPress(object sender, KeyPressEventArgs e)
        {
            if(!char.IsDigit(e.KeyChar) && !char.IsControl(e.KeyChar))
            {
                e.Handled = true;
            }
        }

        private void OnHttp_Click(object sender, EventArgs e)
        {
            this.httpPortTextBox.Enabled = this.httpCheckBox.Checked;
        }

        private void OnHttps_Click(object sender, EventArgs e)
        {
            this.httpsPortTextBox.Enabled = this.httpsCheckBox.Checked;
        }

        private void OnSsh_Click(object sender, EventArgs e)
        {
            this.sshPortTextBox.Enabled = this.sshCheckBox.Checked;
        }

        private void OnMinecraft_Click(object sender, EventArgs e)
        {
            this.minecraftPortTextBox.Enabled = this.minecraftCheckBox.Checked;
        }

        private void OnSave_Click(object sender, EventArgs e)
        {
            this.Kite = new PkKite();

//            kite.Domain = this.domainTextBox.Text;

            this.Kite.Domain = this.domainLabel.Text;

            this.Kite.HTTP.Enabled = this.httpCheckBox.Checked;
            this.Kite.HTTPS.Enabled = this.httpsCheckBox.Checked;
            this.Kite.SSH.Enabled = this.sshCheckBox.Checked;
            this.Kite.Minecraft.Enabled = this.minecraftCheckBox.Checked;

            if (this.Kite.HTTP.Enabled)
            {
                this.Kite.HTTP.Port = this.validatePort(this.httpPortTextBox.Text);

                if (this.Kite.HTTP.Port == 0)
                {
                    PkLogging.Logger(PkLogging.Level.Warning, "Invalid port number", true);
                    return;
                }
            }

            if (this.Kite.HTTPS.Enabled)
            {
                this.Kite.HTTPS.Port = this.validatePort(this.httpsPortTextBox.Text);

                if (this.Kite.HTTPS.Port == 0)
                {
                    PkLogging.Logger(PkLogging.Level.Warning, "Invalid port number", true);
                    return;
                }
            }

            if (this.Kite.SSH.Enabled)
            {
                this.Kite.SSH.Port = this.validatePort(this.sshPortTextBox.Text);

                if (this.Kite.SSH.Port == 0)
                {
                    PkLogging.Logger(PkLogging.Level.Warning, "Invalid port number", true);
                    return;
                }
            }

            if (this.Kite.Minecraft.Enabled)
            {
                this.Kite.Minecraft.Port = this.validatePort(this.minecraftPortTextBox.Text);

                if (this.Kite.Minecraft.Port == 0)
                {
                    PkLogging.Logger(PkLogging.Level.Warning, "Invalid port number", true);
                    return;
                }
            }

 /*           PkKiteUpdateEventArgs args = new PkKiteUpdateEventArgs("save", kite);
            PkKiteUpdateHandler handler = this.KiteUpdated;

            if(handler != null)
            {
                handler(this, args);
            }
            */

            this.DialogResult = DialogResult.OK;
            this.Close();
        }   
            
        private void OnCancel_Click(object sender, EventArgs e)
        {
            this.DialogResult = DialogResult.Cancel;
            this.Close();
        }
    }
}