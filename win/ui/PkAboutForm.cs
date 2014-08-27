using System;
using System.Drawing;
using System.Windows.Forms;
using System.Threading;

namespace Pagekite
{
    public class PkAboutForm : Form
    {
        public PkAboutForm()
        {
            Button okButton       = new Button();
            Label versionLabel    = new Label();
            PictureBox pictureBox = new PictureBox();
            Font font             = new Font("Tahoma", 12);
            Image logo;

            try
            {
                logo = Image.FromFile("img\\pagekite-logo-letters.png");
            }
            catch (Exception e)
            {
                PkLogging.Logger(PkLogging.Level.Error, "Could not locate pagekite logo");
                PkLogging.Logger(PkLogging.Level.Error, "Exception: " + e.Message);
                logo = null;
            }

            if (logo != null)
            {
                pictureBox.Image = logo;
                pictureBox.Height = logo.Height;
                pictureBox.Width = logo.Width;
            }

            this.Size = new Size(380, 460);

            pictureBox.Location = new Point(this.Width / 2 - logo.Width / 2, 20);

            versionLabel.AutoSize = true;
            versionLabel.Font = font;
            versionLabel.Text = "Version: " + PkVersion.Version;
            versionLabel.Location = new Point(this.Width / 2 - versionLabel.Width / 2, 220);

            okButton.Size     = new Size(80, 30);
            okButton.Location = new Point(this.Width / 2 - okButton.Width / 2, 360);
            okButton.Font     = new Font("Tahoma", 10);
            okButton.Text     = "Ok";
            okButton.Click   += new EventHandler(this.OnOkButton_Click);

            this.Controls.Add(pictureBox);
            this.Controls.Add(versionLabel);
            this.Controls.Add(okButton);

            this.MaximizeBox = false;
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.Text = "PageKite";
        }

        private void OnOkButton_Click(object sender, EventArgs e)
        {
            this.Close();
        }
    }
}