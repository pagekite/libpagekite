using System;
using System.Collections.Generic;
using System.Drawing;
using System.Security;
using System.Windows.Forms;

namespace PageKiteUI
{
    public class PkKitePanel : Panel
    {
        private Dictionary<string, PkKite> kiteDictionary;
        private DataGridView kiteDataView;

        public PkKitePanel()
        {
            this.kiteDictionary = new Dictionary<string, PkKite>();
            this.kiteDataView   = new DataGridView();

            this.initDataGrid();

            this.Controls.Add(kiteDataView);

            this.Size       = new Size(560, 180);
            this.BackColor  = Color.White;
            this.AutoScroll = true;
            this.SetStyle(ControlStyles.UserPaint, true);

            // remove ------
            SecureString ok = new SecureString();
            char a = 'a';
            ok.AppendChar(a);
            this.AddKite("wow", "dom", "http", ok, 80);
            this.AddKite("name", "doma", "ssh", ok, 80);
            //--------------
        }

        private void initDataGrid()
        {
            DataGridViewCheckBoxColumn fly     = new DataGridViewCheckBoxColumn();
            DataGridViewTextBoxColumn  name    = new DataGridViewTextBoxColumn();
            DataGridViewTextBoxColumn  domain  = new DataGridViewTextBoxColumn();
            DataGridViewTextBoxColumn  status  = new DataGridViewTextBoxColumn();
            DataGridViewButtonColumn   details = new DataGridViewButtonColumn();

            this.kiteDataView.Location           = new Point(1, 1);
            this.kiteDataView.Size               = new Size(558, 178);
            this.kiteDataView.BackgroundColor    = Color.White;
            this.kiteDataView.BorderStyle        = BorderStyle.None;
            this.kiteDataView.RowHeadersVisible  = false;
            this.kiteDataView.AllowUserToAddRows = false;
            this.kiteDataView.SelectionMode      = DataGridViewSelectionMode.FullRowSelect;

            this.kiteDataView.AdvancedColumnHeadersBorderStyle.All = DataGridViewAdvancedCellBorderStyle.None;
            this.kiteDataView.AdvancedCellBorderStyle.All          = DataGridViewAdvancedCellBorderStyle.None;

            this.kiteDataView.RowTemplate.Height = 30;
            
            this.kiteDataView.ColumnHeadersDefaultCellStyle.Font = new Font("Tahoma", 9, FontStyle.Bold);
            this.kiteDataView.DefaultCellStyle.Font              = new Font("Tahoma", 9);

            fly.HeaderText = "";
            fly.Name       = "Fly";
            fly.Width      = 40;
            fly.TrueValue  = true;
            fly.FalseValue = false;

            name.Name     = "Name";
            name.Width    = 150;
            name.ReadOnly = true;
            name.HeaderCell.Style.Alignment = DataGridViewContentAlignment.MiddleCenter;

            domain.Name     = "Domain";
            domain.Width    = 168;
            domain.ReadOnly = true;
            domain.HeaderCell.Style.Alignment = DataGridViewContentAlignment.MiddleCenter;

            status.Name     = "Status";
            status.Width    = 100;
            status.ReadOnly = true;
            status.HeaderCell.Style.Alignment = DataGridViewContentAlignment.MiddleCenter;

            details.HeaderText = "";
            details.Name       = "Details";
            details.Width      = 100;

            this.kiteDataView.Columns.Add(fly);
            this.kiteDataView.Columns.Add(name);
            this.kiteDataView.Columns.Add(domain);
            this.kiteDataView.Columns.Add(status);
            this.kiteDataView.Columns.Add(details);

            this.kiteDataView.CellClick += new DataGridViewCellEventHandler(this.kiteDataView_CellClick);
        }

        public void AddKite(String name, String domain, String proto, SecureString secret, int port)
        {
            PkKite kite = new PkKite();

            kite.Name   = name;
            kite.Domain = domain;
            kite.Proto  = proto;
            kite.Secret = secret;
            kite.Port   = port;
            kite.Fly    = false;
            
            if (!kiteDictionary.ContainsKey(domain))
            {
                kiteDictionary.Add(domain, kite);
            }

            this.kiteDataView.Rows.Add(false, name, domain, "Grounded", "Details");
        }

        private void kiteDataView_CellClick(object sender, DataGridViewCellEventArgs e)
        {
            if (e.RowIndex < 0 || e.ColumnIndex != this.kiteDataView.Columns["Details"].Index) return;


            //MessageBox.Show(this.kiteDataView.Rows[e.RowIndex].Cells["Domain"].Value.ToString(), "ok");
            //MessageBox.Show(this.kiteDataView.Rows[e.RowIndex].Cells["Fly"].Value.ToString(), "ok");

            string key = this.kiteDataView.Rows[e.RowIndex].Cells["Domain"].Value.ToString();
            PkKiteDetailsForm detailsForm = new PkKiteDetailsForm();
            detailsForm.setDetails(this.kiteDictionary[key]);
            detailsForm.ShowDialog();
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);

            SolidBrush brush = new SolidBrush(Color.DarkGray);
            Pen pen = new Pen(brush, 2);
            e.Graphics.DrawRectangle(pen, e.ClipRectangle);
        }
    }
}