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
            this.kiteTextBox   = new TextBox();
            this.nameTextBox   = new TextBox();
            this.portTextBox   = new TextBox();
            this.secretTextBox = new TextBox();

            FlowLayoutPanel labelFlowPanel    = new FlowLayoutPanel();
            FlowLayoutPanel settingsFlowPanel = new FlowLayoutPanel();

            Font font = new Font("Tahoma", 10);
            Padding labelPadding  = new Padding(2, 12, 2, 12);
            Padding textBoxMargin = new Padding(0, 10, 0, 11);
            int textBoxWidth = 200;

            Label kiteLabel   = makeLabel(font, labelPadding, "PageKite:");
            Label nameLabel   = makeLabel(font, labelPadding, "Name:");
            Label protoLabel  = makeLabel(font, labelPadding, "Protocol:");
            Label portLabel   = makeLabel(font, labelPadding, "Port:");
            Label secretLabel = makeLabel(font, labelPadding, "Secret:");

            this.initTextBoxes(textBoxWidth, font, textBoxMargin);

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
            this.saveButton.Size = new Size(90, 30);
            this.saveButton.Font = font;
            this.saveButton.Text = "Save";

            this.cancelButton.Location = new Point(200, 280);
            this.cancelButton.Size = new Size(90, 30);
            this.cancelButton.Font = font;
            this.cancelButton.Text = "Cancel";

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

        private Label makeLabel(Font font, Padding padding, string text)
        {
            Label label = new Label();
            
            label.AutoSize = true;
            label.Font     = font;
            label.Padding  = padding;
            label.Text     = text;

            return label;
        }

        private void initTextBoxes(int width, Font font, Padding margin)
        {
            this.nameTextBox.Width = width;
            this.nameTextBox.Font = font;
            this.nameTextBox.Margin = margin;

            this.secretTextBox.Width = width;
            this.secretTextBox.Font = font;
            this.secretTextBox.Margin = margin;

            this.kiteTextBox.Width  = width;
            this.kiteTextBox.Font   = font;
            this.kiteTextBox.Margin = margin;

            this.portTextBox.Width  = 80;
            this.portTextBox.Font   = font;
            this.portTextBox.Margin = margin;
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