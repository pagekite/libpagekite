using System;
using System.Drawing;
using System.Windows.Forms;

namespace Pagekite
{
    public class PkOptionsForm : Form
    {
        public bool UsePagekiteNet;
        public bool Debug;
        public bool RememberMe;

        private Button saveButton;
        private Button cancelButton;
        private CheckBox pagekiteNetCheckBox;
        private CheckBox debugCheckBox;
        private CheckBox rememberMeCheckBox;
//        private CheckBox restartOnBootCheckBox; //maybe?

        public PkOptionsForm()
        {
            this.saveButton   = new Button();
            this.cancelButton = new Button();

            Font font = new Font("Tahoma", 10);
            Padding checkBoxMargin = new Padding(0, 20, 0, 5);
            Padding labelMargin    = new Padding(0, 0, 0, 10);

            FlowLayoutPanel flowPanel = new FlowLayoutPanel();
            FlowLayoutPanel buttonPanel = new FlowLayoutPanel();

            this.pagekiteNetCheckBox = this.MakeCheckBox(font, checkBoxMargin, "Use Pagekite.net");
            this.debugCheckBox       = this.MakeCheckBox(font, checkBoxMargin, "Enable debugging");
            this.rememberMeCheckBox  = this.MakeCheckBox(font, checkBoxMargin, "Remember me");

            Label pagekiteNetLabel = this.MakeLabel(font, labelMargin, "Uncheck this if you are running your own front-end relay.");
            Label debugLabel       = this.MakeLabel(font, labelMargin, "For developers, makes the logs far more verbose.");
            Label rememberLabel    = this.MakeLabel(font, labelMargin, "Remember user settings and sign in automatically.");

            flowPanel.Location = new Point(20, 20);
            flowPanel.AutoSize = true;
            flowPanel.FlowDirection = FlowDirection.TopDown;

            buttonPanel.Location = new Point(95, 300);
            buttonPanel.AutoSize = false;
            buttonPanel.FlowDirection = FlowDirection.LeftToRight;

            saveButton.Size = new Size(90, 30);
            saveButton.Font = font;
            saveButton.Text = "Save";
            saveButton.Click += new EventHandler(this.OnSave_Click);

            cancelButton.Size = new Size(90, 30);
            cancelButton.Font = font;
            cancelButton.Text = "Cancel";
            cancelButton.Click += new EventHandler(this.OnCancel_Click);

            flowPanel.Controls.Add(this.pagekiteNetCheckBox);
            flowPanel.Controls.Add(pagekiteNetLabel);
            flowPanel.Controls.Add(this.debugCheckBox);
            flowPanel.Controls.Add(debugLabel);
            flowPanel.Controls.Add(this.rememberMeCheckBox);
            flowPanel.Controls.Add(rememberLabel);

            buttonPanel.Controls.Add(this.saveButton);
            buttonPanel.Controls.Add(this.cancelButton);

            this.Controls.Add(flowPanel);
            this.Controls.Add(buttonPanel);

            this.ClientSize = new Size(380, 360);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.Text = "PageKite - Options";
        }

        public void SetOptions(PkOptions options)
        {
            this.pagekiteNetCheckBox.Checked = options.UsePageKiteNet;
            this.debugCheckBox.Checked       = options.Debug;
            this.rememberMeCheckBox.Checked  = options.RememberMe;
        }

        private CheckBox MakeCheckBox(Font font, Padding margin, string text)
        {
            CheckBox checkBox = new CheckBox();

            checkBox.Font = font;
            checkBox.AutoSize = true;
            checkBox.Margin = margin;
            checkBox.Text = text;

            return checkBox;
        }

        private Label MakeLabel(Font font, Padding margin, string text)
        {
            Label label = new Label();

            label.Font = font;
            label.AutoSize = true;
            label.Margin = margin;
            label.Text = text;

            return label;
        }

        private void OnSave_Click(object sender, EventArgs e)
        {
            this.UsePagekiteNet = this.pagekiteNetCheckBox.Checked;
            this.Debug = this.debugCheckBox.Checked;
            this.RememberMe = this.rememberMeCheckBox.Checked;

            this.DialogResult = DialogResult.OK;
            this.Close();
        }

        private void OnCancel_Click(object sender, EventArgs e)
        {
            this.DialogResult = DialogResult.Cancel;
            this.Close();
        }


/*        private Button   saveButton;
        private Button   defaultsButton;
        private Button   cancelButton;
        private CheckBox allowEvictionCheckBox;
        private CheckBox bailOnErrorCheckBox;
        private CheckBox disableUseCurrentCheckBox;
        private CheckBox frontendPoolCheckBox;
        private CheckBox staticSetupCheckBox;
        private CheckBox useEvilCheckBox;
        private CheckBox useWatchdogCheckBox;
        private TextBox  allowEvictionTextBox;
        private TextBox  bailOnErrorTextBox;
        private TextBox  frontendPoolTextBox;
        private TextBox  maxConnTextBox;
        private TextBox  spareFrontendsTextBox;

        public PkOptionsForm()
        {
            this.saveButton     = new Button();
            this.defaultsButton = new Button();
            this.cancelButton   = new Button();

            Font font = new Font("Tahoma", 10);
            Padding checkBoxMargin = new Padding(0, 10, 0, 10);
            Padding textBoxMargin  = new Padding(0, 8, 0, 0);

            this.allowEvictionCheckBox     = this.MakeCheckBox(font, checkBoxMargin,
                "Allow eviction of streams idle for >");
            this.bailOnErrorCheckBox       = this.MakeCheckBox(font, checkBoxMargin,
                "Bail out (abort) after");
            this.disableUseCurrentCheckBox = this.MakeCheckBox(font, checkBoxMargin,
                "Disable auto-adding current DNS IP as front-end");
            this.frontendPoolCheckBox   = this.MakeCheckBox(font, checkBoxMargin,
                "Use as frontend pool: ");
            this.staticSetupCheckBox       = this.MakeCheckBox(font, checkBoxMargin,
                "Static setup, disable FE failover and DDNS updates");
            this.useEvilCheckBox           = this.MakeCheckBox(font, checkBoxMargin,
                "Evil mode: Very low intervals for reconnect/ping");
            this.useWatchdogCheckBox       = this.MakeCheckBox(font, checkBoxMargin,
                "Enable watchdog thread (dumps core if we lock up)");

            this.allowEvictionTextBox  = this.MakeTextBox(30, font, textBoxMargin, "30");
            this.bailOnErrorTextBox    = this.MakeTextBox(30, font, textBoxMargin, "1");
            this.frontendPoolTextBox   = this.MakeTextBox(180, font, textBoxMargin, 
                "a DNS name");
            this.maxConnTextBox        = this.MakeTextBox(30, font, textBoxMargin, "25");
            this.spareFrontendsTextBox = this.MakeTextBox(30, font, textBoxMargin, "0");

            Label allowEvictionLabel = this.MakeLabel(font, checkBoxMargin, " seconds");
            Label bailOnErrorLabel   = this.MakeLabel(font, checkBoxMargin, " logged errors");
            Label maxConnLabel       = this.MakeLabel(font, checkBoxMargin, 
                "Max connection count:");
            Label spareFrontendsLabel1 = this.MakeLabel(font, checkBoxMargin, "Always connect to");
            Label spareFrontendsLabel2 = this.MakeLabel(font, checkBoxMargin, " spare frontends");

            FlowLayoutPanel flowPanel            = new FlowLayoutPanel();
            FlowLayoutPanel allowEvictionOption  = this.MakeFlowOption(font);
            FlowLayoutPanel bailOnErrorOption    = this.MakeFlowOption(font);
            FlowLayoutPanel frontendPoolOption   = this.MakeFlowOption(font);
            FlowLayoutPanel maxConnOption        = this.MakeFlowOption(font);
            FlowLayoutPanel spareFrontendsOption = this.MakeFlowOption(font);

            flowPanel.Location      = new Point(20, 60);
            flowPanel.Size          = new Size(400, 380);
            flowPanel.FlowDirection = FlowDirection.TopDown;

            this.allowEvictionCheckBox.CheckedChanged += new EventHandler(this.OnAllowEviction_Click);
            this.bailOnErrorCheckBox.CheckedChanged   += new EventHandler(this.OnBailOnError_Click);
            this.frontendPoolCheckBox.CheckedChanged  += new EventHandler(this.OnFrontendPool_Click);

            this.allowEvictionTextBox.Enabled = false;
            this.bailOnErrorTextBox.Enabled   = false;
            this.frontendPoolTextBox.Enabled  = false;

            this.defaultsButton.Location = new Point(250, 20);
            this.defaultsButton.Size     = new Size(130, 30);
            this.defaultsButton.Font     = font;
            this.defaultsButton.Text     = "Restore defaults";

            this.saveButton.Location = new Point(100, 470);
            this.saveButton.Size     = new Size(80, 30);
            this.saveButton.Font     = font;
            this.saveButton.Text     = "Save";
            this.saveButton.Click   += new EventHandler(this.OnSave_Click);

            this.cancelButton.Location = new Point(220, 470);
            this.cancelButton.Size     = new Size(80, 30);
            this.cancelButton.Font     = font;
            this.cancelButton.Text     = "Cancel";
            this.cancelButton.Click   += new EventHandler(this.OnCancel_Click);

            allowEvictionOption.Controls.Add(this.allowEvictionCheckBox);
            allowEvictionOption.Controls.Add(this.allowEvictionTextBox);
            allowEvictionOption.Controls.Add(allowEvictionLabel);

            bailOnErrorOption.Controls.Add(this.bailOnErrorCheckBox);
            bailOnErrorOption.Controls.Add(this.bailOnErrorTextBox);
            bailOnErrorOption.Controls.Add(bailOnErrorLabel);

            frontendPoolOption.Controls.Add(this.frontendPoolCheckBox);
            frontendPoolOption.Controls.Add(this.frontendPoolTextBox);

            maxConnOption.Controls.Add(maxConnLabel);
            maxConnOption.Controls.Add(this.maxConnTextBox);

            spareFrontendsOption.Controls.Add(spareFrontendsLabel1);
            spareFrontendsOption.Controls.Add(this.spareFrontendsTextBox);
            spareFrontendsOption.Controls.Add(spareFrontendsLabel2);

            flowPanel.Controls.Add(this.staticSetupCheckBox);
            flowPanel.Controls.Add(bailOnErrorOption);
            flowPanel.Controls.Add(allowEvictionOption);
            flowPanel.Controls.Add(frontendPoolOption);
            flowPanel.Controls.Add(this.disableUseCurrentCheckBox);
            flowPanel.Controls.Add(this.useWatchdogCheckBox);
            flowPanel.Controls.Add(this.useEvilCheckBox);
            flowPanel.Controls.Add(maxConnOption);
            flowPanel.Controls.Add(spareFrontendsOption);

            this.Controls.Add(this.defaultsButton);
            this.Controls.Add(flowPanel);
            this.Controls.Add(this.saveButton);
            this.Controls.Add(this.cancelButton);

            this.ClientSize = new Size(400, 530);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.Text = "Advanced options";
        }

        private CheckBox MakeCheckBox(Font font, Padding margin, string text)
        {
            CheckBox checkBox = new CheckBox();

            checkBox.Font     = font;
            checkBox.AutoSize = true;
            checkBox.Margin   = margin;
            checkBox.Text     = text;

            return checkBox;
        }

        private TextBox MakeTextBox(int width, Font font, Padding margin, string text)
        {
            TextBox textBox = new TextBox();

            textBox.Width  = width;
            textBox.Font   = font;
            textBox.Margin = margin;
            textBox.Text   = text;

            return textBox;
        }

        private Label MakeLabel(Font font, Padding margin, string text)
        {
            Label label = new Label();

            label.Font = font;
            label.AutoSize = true;
            label.Margin = margin;
            label.Padding = new Padding(0, 1, 0, 0);
            label.Text = text;

            return label;
        }

        private FlowLayoutPanel MakeFlowOption(Font font)
        {
            FlowLayoutPanel layout = new FlowLayoutPanel();

            layout.AutoSize = true;
            layout.Margin = new Padding(0);
            layout.FlowDirection = FlowDirection.LeftToRight;

            return layout;
        }

        private void OnAllowEviction_Click(object sender, EventArgs e)
        {
            if(this.allowEvictionCheckBox.Checked)
            {
                this.allowEvictionTextBox.Enabled = true;
            }
            else
            {
                this.allowEvictionTextBox.Enabled = false;
            }
        }

        private void OnBailOnError_Click(object sender, EventArgs e)
        {
            if(this.bailOnErrorCheckBox.Checked)
            {
                this.bailOnErrorTextBox.Enabled = true;
            }
            else
            {
                this.bailOnErrorTextBox.Enabled = false;
            }
        }

        private void OnFrontendPool_Click(object sender, EventArgs e)
        {
            if(this.frontendPoolCheckBox.Checked)
            {
                this.frontendPoolTextBox.Enabled = true;
            }
            else
            {
                this.frontendPoolTextBox.Enabled = false;
            }
        }

        private void OnSave_Click(object sender, EventArgs e)
        {

        }

        private void OnCancel_Click(object sender, EventArgs e)
        {
            this.Close();
        }
 */
    }
}