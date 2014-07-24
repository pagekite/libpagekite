using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace Pagekite
{
    public class PkAddKiteForm : Form
    {
        private Button cancelButton;
        private Button addButton;
        private ComboBox domainComboBox;
        private Label httpLabel;
        private TextBox domainTextBox;

        public PkAddKiteForm()
        {
            this.cancelButton = new Button();
            this.addButton = new Button();
            this.domainComboBox = new ComboBox();
            this.httpLabel = new Label();
            this.domainTextBox = new TextBox();

            Font font = new Font("Tahoma", 10);
            FlowLayoutPanel flowPanel = new FlowLayoutPanel();

            flowPanel.Location = new Point(30, 40);
            flowPanel.Size = new Size(500, 50);

            this.httpLabel.AutoSize = true;
            this.httpLabel.Font = font;
            this.httpLabel.Margin = new Padding(0, 6, 0, 6);
            this.httpLabel.Text = "http://";

            this.domainTextBox.Width = 180;
            this.domainTextBox.Font = font;
            this.domainTextBox.Text = "";

            this.domainComboBox.Width = 220;
            this.domainComboBox.Font = font;
            this.domainComboBox.DropDownStyle = ComboBoxStyle.DropDownList;

            this.cancelButton.Location = new Point(260, 100);
            this.cancelButton.Size = new Size(80, 30);
            this.cancelButton.Font = font;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.Click += new EventHandler(this.OnCancel_Click);

            this.addButton.Location = new Point(160, 100);
            this.addButton.Size = new Size(80, 30);
            this.addButton.Font = font;
            this.addButton.Text = "Add";

            flowPanel.Controls.Add(this.httpLabel);
            flowPanel.Controls.Add(this.domainTextBox);
            flowPanel.Controls.Add(this.domainComboBox);
            this.Controls.Add(flowPanel);
            this.Controls.Add(this.addButton);
            this.Controls.Add(this.cancelButton);

            this.ClientSize = new Size(500, 150);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.Text = "New Kite";
        }

        public void SetComboBox(Dictionary<string, PkKite> kites)
        {
            this.domainComboBox.Items.Add(".302.is");
            this.domainComboBox.Items.Add(".testing.is");
            this.domainComboBox.Items.Add(".pagekite.me (SSL ok)");

            foreach (string domain in SortByLength(kites.Keys))
            {
                this.domainComboBox.Items.Add("-" + domain + " (SSL ok)");
                this.domainComboBox.Items.Add("." + domain);
            }

            this.domainComboBox.SelectedIndex = 0;
        }

        private IEnumerable<string> SortByLength(IEnumerable<string> list)
        {
            var sorted = from str in list
                         orderby str.Length descending
                         select str;
            return sorted;
        }
        
        private void OnAdd_Click(object sender, EventArgs e)
        {

        }

        private void OnCancel_Click(object sender, EventArgs e)
        {
            this.Close();
        }
    }
}