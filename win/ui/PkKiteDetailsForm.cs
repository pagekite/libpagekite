using System;
using System.Collections.Generic;
using System.Drawing;
using System.Security;
using System.Windows.Forms;

namespace PageKiteUI
{
    public class PkKiteDetailsForm : Form
    {
        private Button   saveButton;
        private Button   cancelButton;
        private ComboBox protoComboBox;
        private TextBox  kiteTextBox;
        private TextBox  nameTextBox;
        private TextBox  portTextBox;
        private TextBox  secretTextBox;

        public PkKiteDetailsForm()
        {
            this.saveButton    = new Button();
            this.cancelButton  = new Button();
            this.protoComboBox = new ComboBox();

            FlowLayoutPanel labelFlowPanel    = new FlowLayoutPanel();
            FlowLayoutPanel settingsFlowPanel = new FlowLayoutPanel();

            Font font = new Font("Tahoma", 10);
            Padding labelPadding  = new Padding(2, 12, 2, 12);
            Padding textBoxMargin = new Padding(0, 10, 0, 11);

            this.kiteTextBox   = this.MakeTextBox(200, font, textBoxMargin);
            this.nameTextBox   = this.MakeTextBox(200, font, textBoxMargin);
            this.portTextBox   = this.MakeTextBox(80, font, textBoxMargin);
            this.secretTextBox = this.MakeTextBox(200, font, textBoxMargin);

            Label kiteLabel   = this.MakeLabel(font, labelPadding, "PageKite:");
            Label nameLabel   = this.MakeLabel(font, labelPadding, "Name:");
            Label protoLabel  = this.MakeLabel(font, labelPadding, "Protocol:");
            Label portLabel   = this.MakeLabel(font, labelPadding, "Port:");
            Label secretLabel = this.MakeLabel(font, labelPadding, "Secret:");

            labelFlowPanel.Location      = new Point(20, 20);
            labelFlowPanel.Size          = new Size(100, 260);
            labelFlowPanel.FlowDirection = FlowDirection.TopDown;

            settingsFlowPanel.Location      = new Point(130, 20);
            settingsFlowPanel.Size          = new Size(200, 260);
            settingsFlowPanel.FlowDirection = FlowDirection.TopDown;

            this.protoComboBox.Width  = 80;
            this.protoComboBox.Font   = font;
            this.protoComboBox.Margin = textBoxMargin;
            this.protoComboBox.Items.AddRange(new object[] {
                "http",
                "ssh"
            });
            this.protoComboBox.DropDownStyle = ComboBoxStyle.DropDownList;

            this.saveButton.Location = new Point(80, 280);
            this.saveButton.Size     = new Size(90, 30);
            this.saveButton.Font     = font;
            this.saveButton.Text     = "Save";
            this.saveButton.Click   += new EventHandler(this.OnSave_Click);

            this.cancelButton.Location = new Point(200, 280);
            this.cancelButton.Size     = new Size(90, 30);
            this.cancelButton.Font     = font;
            this.cancelButton.Text     = "Cancel";
            this.cancelButton.Click   += new EventHandler(this.OnCancel_Click);

            labelFlowPanel.Controls.Add(nameLabel);
            labelFlowPanel.Controls.Add(kiteLabel);
            labelFlowPanel.Controls.Add(secretLabel);
            labelFlowPanel.Controls.Add(protoLabel);
            labelFlowPanel.Controls.Add(portLabel);

            settingsFlowPanel.Controls.Add(this.nameTextBox);
            settingsFlowPanel.Controls.Add(this.kiteTextBox);
            settingsFlowPanel.Controls.Add(this.secretTextBox);
            settingsFlowPanel.Controls.Add(this.protoComboBox);
            settingsFlowPanel.Controls.Add(this.portTextBox);

            this.Controls.Add(labelFlowPanel);
            this.Controls.Add(settingsFlowPanel);
            this.Controls.Add(saveButton);
            this.Controls.Add(cancelButton);

            this.ClientSize      = new Size(360, 340);
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

        private TextBox MakeTextBox(int width, Font font, Padding margin)
        {
            TextBox textBox = new TextBox();

            textBox.Width  = width;
            textBox.Font   = font;
            textBox.Margin = margin;

            return textBox;
        }

        private void OnSave_Click(object sender, EventArgs e)
        {

        }

        private void OnCancel_Click(object sender, EventArgs e)
        {
            this.Close();
        }

        public void setDetails(PkKite kite)
        {
            this.nameTextBox.Text = kite.Name;
            this.kiteTextBox.Text = kite.Domain;
            this.kiteTextBox.Enabled = false;
            this.secretTextBox.Text = "";
            this.protoComboBox.SelectedIndex = kite.Proto.Equals("http") ? 0 : 1;
            this.portTextBox.Text = kite.Port.ToString();
        }
    }
}