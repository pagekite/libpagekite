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

            this.initDataGrid();

            this.Controls.Add(kiteDataView);

            this.Size = new Size(560, 180);
            this.BackColor = Color.White;
            this.AutoScroll = true;
            this.SetStyle(ControlStyles.UserPaint, true);
        }

        private void initDataGrid()
        {
            this.kiteDataView = new DataGridView();
            this.kiteDataView.Location = new Point(1, 1);
            this.kiteDataView.Size = new Size(558, 178);
            this.kiteDataView.RowTemplate.Height = 30;
            this.kiteDataView.BackgroundColor = Color.White;
            this.kiteDataView.BorderStyle = BorderStyle.None;
            this.kiteDataView.RowHeadersVisible = false;
            this.kiteDataView.AdvancedColumnHeadersBorderStyle.All = DataGridViewAdvancedCellBorderStyle.None;
            this.kiteDataView.AdvancedCellBorderStyle.All = DataGridViewAdvancedCellBorderStyle.None;
            this.kiteDataView.SelectionMode = DataGridViewSelectionMode.FullRowSelect;
            this.kiteDataView.AllowUserToAddRows = false;

            DataGridViewCheckBoxColumn fly = new DataGridViewCheckBoxColumn();
            fly.HeaderText = "Fly";
            fly.Name = "Fly";
            fly.HeaderCell.Style.Alignment = DataGridViewContentAlignment.MiddleCenter;
            fly.Width = 40;
            fly.TrueValue = true;
            fly.FalseValue = false;

            DataGridViewTextBoxColumn name = new DataGridViewTextBoxColumn();
            name.Name = "Name";
            name.HeaderCell.Style.Alignment = DataGridViewContentAlignment.MiddleCenter;
            name.Width = 150;
            name.ReadOnly = true;

            DataGridViewTextBoxColumn domain = new DataGridViewTextBoxColumn();
            domain.Name = "Domain";
            domain.HeaderCell.Style.Alignment = DataGridViewContentAlignment.MiddleCenter;
            domain.Width = 168;
            domain.ReadOnly = true;

            DataGridViewTextBoxColumn status = new DataGridViewTextBoxColumn();
            status.Name = "Status";
            status.HeaderCell.Style.Alignment = DataGridViewContentAlignment.MiddleCenter;
            status.Width = 100;
            status.ReadOnly = true;

            DataGridViewButtonColumn details = new DataGridViewButtonColumn();
            details.HeaderText = "";
            details.Name = "Details";
            details.HeaderText = "";
            details.Width = 100;

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
            kite.Name = name;
            kite.Domain = domain;
            kite.Proto = proto;
            kite.Secret = secret;
            kite.Port = port;
            kite.Fly = false;
            
            if (!kiteDictionary.ContainsKey(domain))
            {
                kiteDictionary.Add(domain, kite);
            }

            this.kiteDataView.Rows.Add(false, name, domain, "Grounded", "Details");
        }

        private void kiteDataView_CellClick(object sender, DataGridViewCellEventArgs e)
        {
            if (e.RowIndex < 0 ||
                e.ColumnIndex != this.kiteDataView.Columns["Details"].Index) return;

            //MessageBox.Show(this.kiteDataView.Rows[e.RowIndex].Cells["Domain"].Value.ToString(), "ok");
            MessageBox.Show(this.kiteDataView.Rows[e.RowIndex].Cells["Fly"].Value.ToString(), "ok");

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