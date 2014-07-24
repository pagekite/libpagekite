using System;
using System.Drawing;
using System.Windows.Forms;

namespace Pagekite
{
    public class PkPreferencesForm : Form
    {
        private Button advancedButton;
        private Button saveButton;
        private Button cancelButton;
        private CheckBox rememberCheckBox;
        private GroupBox logGroup;
        private RadioButton errorLogRadio;
        private RadioButton normalLogRadio;
        private RadioButton allLogRadio;

        public PkPreferencesForm()
        {
            this.advancedButton = new Button();
            this.saveButton = new Button();
            this.cancelButton = new Button();
            this.rememberCheckBox = new CheckBox();
            this.logGroup = new GroupBox();
            this.errorLogRadio = new RadioButton();
            this.normalLogRadio = new RadioButton();
            this.allLogRadio = new RadioButton();

            Font font = new Font("Tahoma", 10);
            Padding margin = new Padding(0, 20, 0, 20);

            FlowLayoutPanel flowPanel = new FlowLayoutPanel();
            FlowLayoutPanel logRadioPanel = new FlowLayoutPanel();
            FlowLayoutPanel buttonPanel = new FlowLayoutPanel();

            flowPanel.Location = new Point(20, 20);
            flowPanel.AutoSize = true;
            flowPanel.FlowDirection = FlowDirection.TopDown;

            logRadioPanel.Location = new Point(20, 30);
            logRadioPanel.Size = new Size(150, 90);
            logRadioPanel.FlowDirection = FlowDirection.TopDown;

            buttonPanel.Location = new Point(40, 250);
            buttonPanel.AutoSize = true;
            buttonPanel.FlowDirection = FlowDirection.LeftToRight;

            this.rememberCheckBox.Font = font;
            this.rememberCheckBox.AutoSize = true;
            this.rememberCheckBox.Margin = margin;
            this.rememberCheckBox.Text = "Remember me, sign in automatically at startup";
            this.rememberCheckBox.Checked = true;

            this.errorLogRadio.Font = font;
            this.errorLogRadio.Text = "Only errors";

            this.normalLogRadio.Font = font;
            this.normalLogRadio.Text = "Normal";
            this.normalLogRadio.Checked = true;

            this.allLogRadio.Font = font;
            this.allLogRadio.Text = "All";

            this.logGroup.Size = new Size(360, 130);
            this.logGroup.Font = font;
            this.logGroup.Text = "Log verbosity: ";

            this.saveButton.Size = new Size(80, 30);
            this.saveButton.Font = font;
            this.saveButton.Text = "Save";

            this.cancelButton.Size = new Size(80, 30);
            this.cancelButton.Font = font;
            this.cancelButton.Text = "Cancel";

            this.advancedButton.Size = new Size(130, 30);
            this.advancedButton.Font = font;
            this.advancedButton.Text = "Advanced options";
            this.advancedButton.Click += new EventHandler(this.OnAdvanced_Click);

            logRadioPanel.Controls.Add(errorLogRadio);
            logRadioPanel.Controls.Add(normalLogRadio);
            logRadioPanel.Controls.Add(allLogRadio);

            this.logGroup.Controls.Add(logRadioPanel);

            buttonPanel.Controls.Add(this.saveButton);
            buttonPanel.Controls.Add(this.cancelButton);
            buttonPanel.Controls.Add(this.advancedButton);

            flowPanel.Controls.Add(this.logGroup);
            flowPanel.Controls.Add(this.rememberCheckBox);

            this.Controls.Add(flowPanel);
            this.Controls.Add(buttonPanel);

            this.ClientSize = new Size(400, 300);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.Text = "Options";
            
        }

        private void OnAdvanced_Click(object sender, EventArgs e)
        {
            PkOptionsForm options = new PkOptionsForm();
            options.ShowDialog();
        }
    }
}