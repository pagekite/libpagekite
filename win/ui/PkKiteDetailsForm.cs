using System;
using System.Collections.Generic;
using System.Drawing;
using System.Security;
using System.Windows.Forms;

namespace Pagekite
{
    public class PkKiteDetailsForm : Form
    {

        public delegate void PkKiteUpdateHandler(
            object sender, PkKiteUpdateEventArgs e);

        public event PkKiteUpdateHandler KiteUpdated;

        private Button   saveButton;
        private Button   cancelButton;
        private CheckBox httpCheckBox;
        private CheckBox httpsCheckBox;
        private CheckBox sshCheckBox;
        private CheckBox minecraftCheckBox;
        private TextBox  domainTextBox;
        private TextBox  httpPortTextBox;
        private TextBox  httpsPortTextBox;
        private TextBox  sshPortTextBox;
        private TextBox  minecraftPortTextBox;

        public PkKiteDetailsForm()
        {
            this.saveButton    = new Button();
            this.cancelButton  = new Button();

            FlowLayoutPanel labelFlowPanel    = new FlowLayoutPanel();
            FlowLayoutPanel settingsFlowPanel = new FlowLayoutPanel();

            Font font = new Font("Tahoma", 10);
            Padding checkBoxMargin  = new Padding(2, 12, 2, 12);
            Padding textBoxMargin   = new Padding(0, 10, 0, 11);

            this.domainTextBox        = this.MakeTextBox(200, font, textBoxMargin);
            this.httpPortTextBox      = this.MakeTextBox(80, font, textBoxMargin);
            this.httpsPortTextBox     = this.MakeTextBox(80, font, textBoxMargin);
            this.sshPortTextBox       = this.MakeTextBox(80, font, textBoxMargin);
            this.minecraftPortTextBox = this.MakeTextBox(80, font, textBoxMargin);

            this.httpCheckBox      = this.MakeCheckBox(font, checkBoxMargin, " HTTP");
            this.httpsCheckBox     = this.MakeCheckBox(font, checkBoxMargin, " HTTPS");
            this.sshCheckBox       = this.MakeCheckBox(font, checkBoxMargin, " SSH");
            this.minecraftCheckBox = this.MakeCheckBox(font, checkBoxMargin, " Minecraft");

            Label domainLabel = this.MakeLabel(font, checkBoxMargin, "Domain:");
            Label protoLabel  = this.MakeLabel(font, checkBoxMargin, "Protocol:");
            Label portLabel   = this.MakeLabel(font, checkBoxMargin, "Local port:");
   //         Label protoLabel  = this.MakeLabel(font, labelPadding, "Protocol:");
   //        Label portLabel   = this.MakeLabel(font, labelPadding, "Port:");

            labelFlowPanel.Location      = new Point(20, 20);
            labelFlowPanel.Size          = new Size(140, 300);
            labelFlowPanel.FlowDirection = FlowDirection.TopDown;

            settingsFlowPanel.Location      = new Point(160, 20);
            settingsFlowPanel.Size          = new Size(200, 300);
            settingsFlowPanel.FlowDirection = FlowDirection.TopDown;

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
            

/*            this.protoComboBox.Width  = 80;
            this.protoComboBox.Font   = font;
            this.protoComboBox.Margin = textBoxMargin;
            this.protoComboBox.Items.AddRange(new object[] {
                "HTTP",
                "HTTPS",
                "SSH"
            });
            this.protoComboBox.DropDownStyle = ComboBoxStyle.DropDownList;
*/
            this.saveButton.Location = new Point(80, 320);
            this.saveButton.Size     = new Size(90, 30);
            this.saveButton.Font     = font;
            this.saveButton.Text     = "Save";
            this.saveButton.Click   += new EventHandler(this.OnSave_Click);

            this.cancelButton.Location = new Point(200, 320);
            this.cancelButton.Size     = new Size(90, 30);
            this.cancelButton.Font     = font;
            this.cancelButton.Text     = "Cancel";
            this.cancelButton.Click   += new EventHandler(this.OnCancel_Click);

            labelFlowPanel.Controls.Add(domainLabel);
            labelFlowPanel.Controls.Add(protoLabel);
  /*          labelFlowPanel.Controls.Add(portLabel); */
            labelFlowPanel.Controls.Add(this.httpCheckBox);
            labelFlowPanel.Controls.Add(this.httpsCheckBox);
            labelFlowPanel.Controls.Add(this.sshCheckBox);
            labelFlowPanel.Controls.Add(this.minecraftCheckBox);

            settingsFlowPanel.Controls.Add(this.domainTextBox);
            settingsFlowPanel.Controls.Add(portLabel);
//            settingsFlowPanel.Controls.Add(this.protoComboBox);
            settingsFlowPanel.Controls.Add(this.httpPortTextBox);
            settingsFlowPanel.Controls.Add(this.httpsPortTextBox);
            settingsFlowPanel.Controls.Add(this.sshPortTextBox);
            settingsFlowPanel.Controls.Add(this.minecraftPortTextBox);

            this.Controls.Add(labelFlowPanel);
            this.Controls.Add(settingsFlowPanel);
            this.Controls.Add(saveButton);
            this.Controls.Add(cancelButton);

            this.ClientSize      = new Size(380, 380);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox     = false;
            this.StartPosition   = FormStartPosition.CenterScreen;
            this.Text            = "Kite details";
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
            PkKite kite = new PkKite();

            kite.Domain = this.domainTextBox.Text;

            kite.HTTP.Enabled      = this.httpCheckBox.Checked;
            kite.HTTPS.Enabled     = this.httpsCheckBox.Checked;
            kite.SSH.Enabled       = this.sshCheckBox.Checked;
            kite.Minecraft.Enabled = this.minecraftCheckBox.Checked;

            if (kite.HTTP.Enabled)
            {
                kite.HTTP.Port = this.validatePort(this.httpPortTextBox.Text);

                if(kite.HTTP.Port == 0)
                {
                    //invalid port number
                    return;
                }
            }

            if (kite.HTTPS.Enabled)
            {
                kite.HTTPS.Port = this.validatePort(this.httpsPortTextBox.Text);

                if (kite.HTTPS.Port == 0)
                {
                    //invalid port number
                    return;
                }
            }

            if(kite.SSH.Enabled)
            {
                kite.SSH.Port = this.validatePort(this.sshPortTextBox.Text);

                if (kite.SSH.Port == 0)
                {
                    //invalid port number
                    return;
                }
            }

            if (kite.Minecraft.Enabled)
            {
                kite.Minecraft.Port = this.validatePort(this.minecraftPortTextBox.Text);

                if (kite.Minecraft.Port == 0)
                {
                    //invalid port number
                    return;
                }
            }

            PkKiteUpdateEventArgs args = new PkKiteUpdateEventArgs("save", kite);
            PkKiteUpdateHandler handler = this.KiteUpdated;

            if(handler != null)
            {
                handler(this, args);
            }

            this.Close();
        }   
            
        private void OnCancel_Click(object sender, EventArgs e)
        {
            this.Close();
        }

        public void setDetails(PkKite kite)
        {
            this.domainTextBox.Text = kite.Domain;
            this.domainTextBox.Enabled = false;
            
            this.httpCheckBox.Checked      = kite.HTTP.Enabled;
            this.httpsCheckBox.Checked     = kite.HTTPS.Enabled;
            this.sshCheckBox.Checked       = kite.SSH.Enabled;
            this.minecraftCheckBox.Checked = kite.Minecraft.Enabled;

            this.httpPortTextBox.Enabled      = kite.HTTP.Enabled;
            this.httpsPortTextBox.Enabled     = kite.HTTPS.Enabled;
            this.sshPortTextBox.Enabled       = kite.SSH.Enabled;
            this.minecraftPortTextBox.Enabled = kite.Minecraft.Enabled;

            this.httpPortTextBox.Text      = kite.HTTP.Port.ToString();
            this.httpsPortTextBox.Text     = kite.HTTPS.Port.ToString();
            this.sshPortTextBox.Text       = kite.SSH.Port.ToString();
            this.minecraftPortTextBox.Text = kite.Minecraft.Port.ToString();
        }
    }
}