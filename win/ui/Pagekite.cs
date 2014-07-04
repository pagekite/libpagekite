using System;
using System.Data;
using System.Drawing;
using System.Windows.Forms;

namespace PageKiteUI
{
    public class PageKite : Form
    {
        private Button      optionsButton;
        private Button      addButton;
        private Button      removeButton;
        private Button      flyButton;
        private Button      groundButton;
        private ContextMenu trayMenu;
        private Label       kitesLabel;
        private MenuStrip   menuStrip;
        private NotifyIcon  trayIcon;
        private PkKitePanel kitePanel;

        public PageKite()
        {
            this.Init();
        }

        public void Init()
        {
            this.optionsButton = new Button();
            this.addButton     = new Button();
            this.removeButton  = new Button();
            this.flyButton     = new Button();
            this.groundButton  = new Button();
            this.trayMenu      = new ContextMenu();
            this.kitesLabel    = new Label();
            this.menuStrip     = new MenuStrip();
            this.trayIcon      = new NotifyIcon();
            this.kitePanel     = new PkKitePanel();

            string fontName = "Tahoma";

            this.CreateMenuStrip();
            this.CreateSystemTrayIcon();

            this.kitesLabel.Location = new Point(20, 50);
            this.kitesLabel.AutoSize = true;
            this.kitesLabel.Font     = new Font(fontName, 10);
            this.kitesLabel.Text     = "Your Kites:";

            this.optionsButton.Location = new Point(500, 40);
            this.optionsButton.Size     = new Size(80, 30);
            this.optionsButton.Font     = new Font(fontName, 10);
            this.optionsButton.Text     = "Options";
            this.optionsButton.Click   += new EventHandler(this.OnOptions_Click);

            this.kitePanel.Location = new Point(20, 80);

            this.addButton.Location = new Point(20, 270);
            this.addButton.Size     = new Size(50, 30);
            this.addButton.Font     = new Font(fontName, 12);
            this.addButton.Text     = "+";

            this.removeButton.Location = new Point(80, 270);
            this.removeButton.Size     = new Size(50, 30);
            this.removeButton.Font     = new Font(fontName, 12);
            this.removeButton.Text     = "-";

            this.flyButton.Location = new Point(410, 270);
            this.flyButton.Size     = new Size(80, 30);
            this.flyButton.Font     = new Font(fontName, 10);
            this.flyButton.Text     = "Fly";

            this.groundButton.Location = new Point(500, 270);
            this.groundButton.Size     = new Size(80, 30);
            this.groundButton.Font     = new Font(fontName, 10);
            this.groundButton.Enabled  = false;
            this.groundButton.Text     = "Ground";

            this.Controls.Add(this.menuStrip);
            this.Controls.Add(this.kitesLabel);
            this.Controls.Add(this.optionsButton);
            this.Controls.Add(this.kitePanel);
            this.Controls.Add(this.addButton);
            this.Controls.Add(this.removeButton);
            this.Controls.Add(this.flyButton);
            this.Controls.Add(this.groundButton);

            this.ClientSize      = new Size(600, 500);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox     = false;
            this.StartPosition   = FormStartPosition.CenterScreen;
            this.Text            = "PageKite";
        }

        private void CreateMenuStrip()
        {
            ToolStripMenuItem exitMenuItem = new ToolStripMenuItem();
            ToolStripMenuItem fileMenuItem = new ToolStripMenuItem();

            exitMenuItem.Text   = "Exit";
            exitMenuItem.Click += new EventHandler(this.OnExit);

            fileMenuItem.DropDownItems.AddRange(new ToolStripItem[] {
                exitMenuItem
            });
            fileMenuItem.Text = "File";

            this.menuStrip.Items.AddRange(new ToolStripItem[] {
                fileMenuItem
            });
            this.menuStrip.Location  = new Point(0, 0);
            this.menuStrip.BackColor = SystemColors.Control;
            this.menuStrip.TabIndex  = 0;
        }

        private void CreateSystemTrayIcon()
        {
            this.trayMenu.MenuItems.Add("About...", OnAbout);
            this.trayMenu.MenuItems.Add("Exit", OnExit);

            this.trayIcon.Icon = new Icon("smiley.ico", 40, 40);
            this.trayIcon.Text = "PageKite";

            this.trayIcon.ContextMenu = trayMenu;
            this.trayIcon.Visible     = true;
        }

        private void OnOptions_Click(object sender, EventArgs e)
        {
            PkOptionsForm options = new PkOptionsForm();
            options.ShowDialog();
        }

        private void OnAbout(object sender, EventArgs e)
        {

        }

        private void OnExit(object sender, EventArgs e)
        {
            Application.Exit();
        }

        protected override void Dispose(bool disposing)
        {
            if(disposing)
            {
                if(this.trayIcon != null)
                {
                    this.trayIcon.Visible = false;
                    this.trayIcon.Icon    = null;
                    this.trayIcon.Dispose();
                    this.trayIcon = null;
                }
            }
            
            base.Dispose(disposing);
        }

        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.Run(new PageKite());
        }
    }
}
