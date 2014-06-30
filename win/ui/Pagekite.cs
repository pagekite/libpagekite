using System;
using System.Data;
using System.Drawing;
using System.Windows.Forms;

namespace PageKiteUI
{
    public class PageKite : Form
    {
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.Run(new PageKite());
        }

        private ContextMenu trayMenu;
       // private Label welcomeLabel;
        private Label kitesLabel;
        private Button optionsButton;
        private Button addButton;
        private Button removeButton;
        private Button flyButton;
        private Button groundButton;
        private MenuStrip menuStrip;
        private NotifyIcon trayIcon;
        private PkKitePanel kitePanel;
        private ToolStripMenuItem exitMenuItem;
        private ToolStripMenuItem fileMenuItem;

        public PageKite()
        {
            this.Init();
        }

        public void Init()
        {
            String fontName = "Arial";
            int tabIndex = 1;

            this.CreateMenuStrip();
            this.CreateSystemTrayIcon();

       /*     this.welcomeLabel = new Label();
            this.welcomeLabel.Location = new Point(20, 40);
            this.welcomeLabel.AutoSize = true;
            this.welcomeLabel.Font = new Font(fontName, 16);
            this.welcomeLabel.Text = "Welcome!"; */

            this.kitesLabel = new Label();
            this.kitesLabel.Location = new Point(20, 125);
            this.kitesLabel.AutoSize = true;
            this.kitesLabel.Font = new Font(fontName, 10);
            this.kitesLabel.Text = "Your Kites:";

            this.optionsButton = new Button();
            this.optionsButton.Location = new Point(400, 100);
            this.optionsButton.Size = new Size(80, 40);
            this.optionsButton.Font = new Font(fontName, 10);
            this.optionsButton.TabIndex = tabIndex++;
            this.optionsButton.Text = "Options";
            this.optionsButton.Click += new EventHandler(this.OnOptions_Click);

            this.kitePanel = new PkKitePanel();
            this.kitePanel.Location = new Point(20, 160);

            this.addButton = new Button();
            this.addButton.Location = new Point(20, 360);
            this.addButton.Size = new Size(50, 40);
            this.addButton.Font = new Font(fontName, 12);
            this.addButton.TabIndex = tabIndex++;
            this.addButton.Text = "+";

            this.removeButton = new Button();
            this.removeButton.Location = new Point(80, 360);
            this.removeButton.Size = new Size(50, 40);
            this.removeButton.Font = new Font(fontName, 12);
            this.removeButton.TabIndex = tabIndex++;
            this.removeButton.Text = "-";

            this.flyButton = new Button();
            this.flyButton.Location = new Point(310, 360);
            this.flyButton.Size = new Size(80, 40);
            this.flyButton.Font = new Font(fontName, 10);
            this.flyButton.TabIndex = tabIndex++;
            this.flyButton.Text = "Fly";

            this.groundButton = new Button();
            this.groundButton.Location = new Point(400, 360);
            this.groundButton.Size = new Size(80, 40);
            this.groundButton.Font = new Font(fontName, 10);
            this.groundButton.TabIndex = tabIndex++;
            this.groundButton.Enabled = false;
            this.groundButton.Text = "Ground";

            this.Controls.Add(this.menuStrip);
   //         this.Controls.Add(this.welcomeLabel);
            this.Controls.Add(this.kitesLabel);
            this.Controls.Add(this.optionsButton);
            this.Controls.Add(this.kitePanel);
            this.Controls.Add(this.addButton);
            this.Controls.Add(this.removeButton);
            this.Controls.Add(this.flyButton);
            this.Controls.Add(this.groundButton);

            this.ClientSize = new Size(500, 420);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.Text = "PageKite";
        }

        private void CreateMenuStrip()
        {
            this.exitMenuItem = new ToolStripMenuItem();
            this.exitMenuItem.Text = "Exit";
            this.exitMenuItem.Click += new EventHandler(this.OnExit);

            this.fileMenuItem = new ToolStripMenuItem();
            this.fileMenuItem.DropDownItems.AddRange(new ToolStripItem[] {
                this.exitMenuItem
            });
            this.fileMenuItem.Text = "File";

            this.menuStrip = new MenuStrip();
            this.menuStrip.Items.AddRange(new ToolStripItem[] {
                this.fileMenuItem
            });
            this.menuStrip.Location = new Point(0, 0);
            this.menuStrip.BackColor = SystemColors.Control;
            this.menuStrip.TabIndex = 0;
        }

        private void CreateSystemTrayIcon()
        {
            this.trayMenu = new ContextMenu();
            this.trayMenu.MenuItems.Add("About...", OnAbout);
            this.trayMenu.MenuItems.Add("Exit", OnExit);

            this.trayIcon = new NotifyIcon();
            this.trayIcon.Icon = new Icon("smiley.ico", 40, 40);
            this.trayIcon.Text = "PageKite";

            this.trayIcon.ContextMenu = trayMenu;
            this.trayIcon.Visible = true;
        }

        private void OnOptions_Click(object sender, EventArgs e)
        {

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
                    this.trayIcon.Icon = null;
                    this.trayIcon.Dispose();
                    this.trayIcon = null;
                }
            }
            
            base.Dispose(disposing);
        }
    }
}
