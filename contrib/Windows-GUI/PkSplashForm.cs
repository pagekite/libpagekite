using System;
using System.Drawing;
using System.Windows.Forms;
using System.Threading;

namespace Pagekite
{
    public class PkSplashForm : Form
    {
        private delegate void CloseDelegate();
        private static PkSplashForm splashForm;

        public PkSplashForm()
        {
            PictureBox pictureBox = new PictureBox();
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
                this.Size = new Size(logo.Width, logo.Height);
                this.Controls.Add(pictureBox);
            }

            this.StartPosition = FormStartPosition.CenterScreen;
            this.FormBorderStyle = FormBorderStyle.None;
        }

        static public void ShowSplashScreen()
        {
            if (splashForm != null)
            {
                return;
            }

            Thread thread = new Thread(new ThreadStart(PkSplashForm.ShowForm));
            thread.IsBackground = true;

            try
            {
                thread.Start();
            }
            catch(Exception e)
            {
                PkLogging.Logger(PkLogging.Level.Error, "Unable to start SplashForm");
                PkLogging.Logger(PkLogging.Level.Error, e.Message);
            }
        }

        static private void ShowForm()
        {
            splashForm = new PkSplashForm();
            Application.Run(splashForm);
        }

        static public void CloseSplashScreen()
        {
            if (splashForm == null)
            {
                return;
            }

            splashForm.Invoke(new CloseDelegate(PkSplashForm.CloseFormInternal));
        }

        static private void CloseFormInternal()
        {
            splashForm.Close();
            splashForm = null;
        }
    }
}