using System;
using System.Drawing;
using System.Windows.Forms;

namespace Pagekite
{
    public class PkLogPanel : Panel
    {
        public delegate void UpdateDelegate(object sender, EventArgs e);
        private TextBox logTextBox;

        public PkLogPanel()
        {
            this.logTextBox = new TextBox();

            this.logTextBox.Size = new Size(630, 230);
            this.logTextBox.Font = new Font("Tahoma", 10);
            this.logTextBox.ScrollBars = ScrollBars.Both;
            this.logTextBox.WordWrap   = false;
            this.logTextBox.BackColor  = Color.White;
            this.logTextBox.Multiline  = true;
            this.logTextBox.ReadOnly   = true;

            this.Controls.Add(logTextBox);

            this.SetStyle(ControlStyles.OptimizedDoubleBuffer, true);
            this.Size = new Size(630, 230);
        }

        public void UpdateLog(object sender, EventArgs e)
        {
            // Making this thread safe
            if(this.InvokeRequired)
            {
                UpdateDelegate updateDelgate = new UpdateDelegate(this.UpdateLog);
                this.Invoke(updateDelgate, new object[] { sender, e });
                return;
            }

            this.logTextBox.Text = "";
            this.logTextBox.AppendText(PkLogging.Log);
        }
    }
}