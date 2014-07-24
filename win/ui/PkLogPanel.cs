using System;
using System.Drawing;
using System.Windows.Forms;

namespace Pagekite
{
    public class PkLogPanel : Panel
    {
        private TextBox logTextBox;

        public PkLogPanel()
        {
            this.logTextBox = new TextBox();

            this.logTextBox.Size = new Size(630, 230);
            this.logTextBox.Font = new Font("Tahoma", 10);
            this.logTextBox.ScrollBars = ScrollBars.Both;
            this.logTextBox.WordWrap = false;
            this.logTextBox.BackColor = Color.White;
            this.logTextBox.Multiline = true;
            this.logTextBox.ReadOnly = true;

            this.Controls.Add(logTextBox);

            this.Size = new Size(630, 230);
        }

        public void setLog(string log)
        {
            this.logTextBox.Text = log; //remove?
        }

        public void Update(object sender, EventArgs e)
        {
            this.logTextBox.AppendText(PkLogging.GetLog());
        }
    }
}