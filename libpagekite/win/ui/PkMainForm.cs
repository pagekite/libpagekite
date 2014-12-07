using System;
using System.Data;
using System.Drawing;
using System.Windows.Forms;

namespace Pagekite
{
    public partial class PkMainForm : Form
    {
        private Button addButton;
        private Button removeButton;
        private Button flyButton;
        private Button groundButton;
        private ContextMenu trayMenu;
        private GroupBox mainBox;
        private Label daysLeftLabel;
        private MenuStrip menuStrip;
        private MenuItem flyMenuItem;
        private MenuItem groundMenuItem;
        private NotifyIcon trayIcon;
        private PkLogPanel logPanel;
        private PkKitePanel kitePanel;

        private void InitForm()
        {
            this.addButton      = new Button();
            this.removeButton   = new Button();
            this.flyButton      = new Button();
            this.groundButton   = new Button();
            this.trayMenu       = new ContextMenu();
            this.daysLeftLabel  = new Label();
            this.menuStrip      = new MenuStrip();
            this.flyMenuItem    = new MenuItem();
            this.groundMenuItem = new MenuItem();
            this.trayIcon       = new NotifyIcon();
            this.logPanel       = new PkLogPanel();
            this.kitePanel      = new PkKitePanel();
            this.mainBox        = new GroupBox();

            string fontName = "Tahoma";

            this.mainBox.Location = new Point(10, 40);
            this.mainBox.Size     = new Size(630, 270);
            this.mainBox.Font     = new Font(fontName, 10);
            this.mainBox.Text     = "Your Kites: ";

            this.CreateMenuStrip();
            this.CreateSystemTrayIcon();

            this.kitePanel.Location = new Point(20, 30);
            this.kitePanel.Size     = new Size(590, 190);
            this.kitePanel.KiteDetail_Click += new PkKitePanel.PkKiteUpdateHandler(this.OnShowDetails_Click);

            this.daysLeftLabel.Location = new Point(530, 25);
            this.daysLeftLabel.AutoSize = true;
            this.daysLeftLabel.Font     = new Font(fontName, 10);

            this.addButton.Location = new Point(20, 230);
            this.addButton.Size     = new Size(50, 30);
            this.addButton.Font     = new Font(fontName, 12);
            this.addButton.Text     = "+";
            this.addButton.Click   += new EventHandler(this.OnAdd_Click);

            this.removeButton.Location = new Point(80, 230);
            this.removeButton.Size     = new Size(50, 30);
            this.removeButton.Font     = new Font(fontName, 12);
            this.removeButton.Text     = "-";
            this.removeButton.Click   += new EventHandler(this.OnRemove_Click);

            this.flyButton.Location = new Point(440, 230);
            this.flyButton.Size     = new Size(80, 30);
            this.flyButton.Font     = new Font(fontName, 10);
            this.flyButton.Text     = "Fly";
            this.flyButton.Click   += new EventHandler(this.OnFly_Click);

            this.groundButton.Location = new Point(530, 230);
            this.groundButton.Size     = new Size(80, 30);
            this.groundButton.Font     = new Font(fontName, 10);
            this.groundButton.Enabled  = false;
            this.groundButton.Text     = "Ground";
            this.groundButton.Click   += new EventHandler(this.OnGround_Click);

            this.logPanel.Location = new Point(10, 320);

            this.mainBox.Controls.Add(this.kitePanel);
            this.mainBox.Controls.Add(this.addButton);
            this.mainBox.Controls.Add(this.removeButton);
            this.mainBox.Controls.Add(this.flyButton);
            this.mainBox.Controls.Add(this.groundButton);

            this.Controls.Add(this.menuStrip);
            this.Controls.Add(this.daysLeftLabel);
            this.Controls.Add(this.mainBox);
            this.Controls.Add(this.logPanel);

            this.ClientSize      = new Size(650, 560);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox     = false;
            this.StartPosition   = FormStartPosition.CenterScreen;
            this.Text            = "PageKite";
            this.FormClosing    += new FormClosingEventHandler(this.PkMainForm_FormClosing);
            this.Resize         += new EventHandler(this.PkMainForm_Resize);
        }

        private void CreateMenuStrip()
        {
            ToolStripMenuItem toolsMenu = new ToolStripMenuItem();
            ToolStripMenuItem fileMenu = new ToolStripMenuItem();
            ToolStripMenuItem helpMenu = new ToolStripMenuItem();

            ToolStripMenuItem aboutMenuItem    = new ToolStripMenuItem();
            ToolStripMenuItem exitMenuItem     = new ToolStripMenuItem();
            ToolStripMenuItem logOutMenuItem   = new ToolStripMenuItem();
            ToolStripMenuItem optionsMenuItem  = new ToolStripMenuItem();
            ToolStripMenuItem viewHelpMenuItem = new ToolStripMenuItem();
            ToolStripMenuItem pagekiteMenuItem = new ToolStripMenuItem();

            ToolStripSeparator exitSeperator = new ToolStripSeparator();
            ToolStripSeparator helpSeperator = new ToolStripSeparator();

            aboutMenuItem.Text   = "About";
            aboutMenuItem.Click += new EventHandler(this.OnAbout_Click);

            exitMenuItem.Text   = "Exit";
            exitMenuItem.Click += new EventHandler(this.OnExit_Click);

            logOutMenuItem.Text   = "Log Out";
            logOutMenuItem.Click += new EventHandler(this.OnLogOut_Click);

            optionsMenuItem.Text   = "Options";
            optionsMenuItem.Click += new EventHandler(this.OnOptions_Click);

            viewHelpMenuItem.Text   = "View Help";
            viewHelpMenuItem.Click += new EventHandler(this.OnHelp_Click);

            pagekiteMenuItem.Text   = "Open Pagekite.net";
            pagekiteMenuItem.Click += new EventHandler(this.OnPagekiteNet_Click);

            fileMenu.DropDownItems.AddRange(new ToolStripItem[] {
                logOutMenuItem,
                exitSeperator,
                exitMenuItem
            });
            fileMenu.Text = "File";

            toolsMenu.DropDownItems.AddRange(new ToolStripItem[] {
                optionsMenuItem
            });
            toolsMenu.Text = "Tools";

            helpMenu.DropDownItems.AddRange(new ToolStripItem[] {
                viewHelpMenuItem,
                pagekiteMenuItem,
                helpSeperator,
                aboutMenuItem
            });
            helpMenu.Text = "Help";

            this.menuStrip.Items.AddRange(new ToolStripItem[] {
                fileMenu,
                toolsMenu,
                helpMenu,
            });
            this.menuStrip.Location = new Point(0, 0);
            this.menuStrip.BackColor = SystemColors.Control;
            this.menuStrip.TabIndex = 0;
        }

        private void CreateSystemTrayIcon()
        {
            flyMenuItem.Text   = "Fly";
            flyMenuItem.Click += new EventHandler(this.OnFlyMenuItem_Click);

            groundMenuItem.Text   = "Ground";
            groundMenuItem.Click += new EventHandler(this.OnGroundMenuItem_Click);
            groundMenuItem.Enabled = false;

            this.trayMenu.MenuItems.Add("Open PageKite", this.OnOpen_Click);
            this.trayMenu.MenuItems.Add("-");
            this.trayMenu.MenuItems.Add(flyMenuItem);
            this.trayMenu.MenuItems.Add(groundMenuItem);
            this.trayMenu.MenuItems.Add("-");
            this.trayMenu.MenuItems.Add("Options", OnOptions_Click);
            this.trayMenu.MenuItems.Add("About...", OnAbout_Click);
            this.trayMenu.MenuItems.Add("-");
            this.trayMenu.MenuItems.Add("Exit", OnExit_Click);

            this.trayIcon.Icon = new Icon("img\\favicon16.ico");
            this.trayIcon.Text = "PageKite";
            this.trayIcon.ContextMenu = trayMenu;
            this.trayIcon.Visible = true;
            this.trayIcon.MouseDoubleClick += new MouseEventHandler(this.OnTrayIcon_DoubleClick);
        }

        private void OnExit_Click(object sender, EventArgs e)
        {
            Application.Exit();
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                if (this.trayIcon != null)
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