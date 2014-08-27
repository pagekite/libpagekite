using System;
using System.Collections.Generic;
using System.Drawing;
using System.Security;
using System.Windows.Forms;

namespace Pagekite
{
    public class PkKitePanel : Panel
    {
        public delegate void PkKiteUpdateHandler(
            object sender, PkKiteUpdateEventArgs e);

        public event PkKiteUpdateHandler KiteDetail_Click;

        private DataGridView kiteDataView;

        public PkKitePanel()
        {
            this.kiteDataView   = new DataGridView();

            this.InitDataGrid();

            this.Controls.Add(kiteDataView);

            this.BackColor  = Color.White;
            this.AutoScroll = true;
            this.SetStyle(ControlStyles.UserPaint, true);
        }

        private void InitDataGrid()
        {
            DataGridViewCheckBoxColumn fly     = new DataGridViewCheckBoxColumn();
            DataGridViewTextBoxColumn  domain  = new DataGridViewTextBoxColumn();
            DataGridViewTextBoxColumn  status  = new DataGridViewTextBoxColumn();
            DataGridViewButtonColumn   details = new DataGridViewButtonColumn();

            this.kiteDataView.Location           = new Point(1, 1);
            this.kiteDataView.Size               = new Size(588, 178);
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

            domain.Name     = "Domain";
            domain.Width    = 288;
            domain.ReadOnly = true;
            domain.HeaderCell.Style.Alignment = DataGridViewContentAlignment.MiddleCenter;

            status.Name     = "Status";
            status.Width    = 160;
            status.ReadOnly = true;
            status.HeaderCell.Style.Alignment = DataGridViewContentAlignment.MiddleCenter;

            details.HeaderText = "";
            details.Name       = "Details";
            details.Width      = 100;

            this.kiteDataView.Columns.Add(fly);
            this.kiteDataView.Columns.Add(domain);
            this.kiteDataView.Columns.Add(status);
            this.kiteDataView.Columns.Add(details);

            this.kiteDataView.CellClick += new DataGridViewCellEventHandler(this.kiteDataView_CellClick);
        }

        public void AddKites(Dictionary<string, PkKite> kites)
        {
            foreach (PkKite kite in kites.Values)
            {
                this.kiteDataView.Rows.Add(false, kite.Domain, "Grounded", "Details");
            }
        }

        public void AddKite(PkKite kite)
        {
            this.kiteDataView.Rows.Add(false, kite.Domain, "Grounded", "Details");
        }

        public void EnableControls()
        {
            DataGridViewCheckBoxColumn check = (DataGridViewCheckBoxColumn)this.kiteDataView.Columns["Fly"];
            check.ReadOnly = false;
            check.FlatStyle = FlatStyle.Standard;
            check.DefaultCellStyle.ForeColor = Color.Gray;

            DataGridViewButtonColumn ok = (DataGridViewButtonColumn)this.kiteDataView.Columns["Details"];
            ok.ReadOnly = false;
            ok.FlatStyle = FlatStyle.Standard;
            ok.DefaultCellStyle.ForeColor = Color.Gray;
        //    ok.DefaultCellStyle.
//            ok.DefaultCellStyle.BackColor = Color.DarkGray;
        }

        public void DisableControls()
        {
     /*       this.kiteDataView.Enabled = false;
            this.kiteDataView.ForeColor = Color.Gray;
            */
            DataGridViewCheckBoxColumn check = (DataGridViewCheckBoxColumn)this.kiteDataView.Columns["Fly"];
            check.ReadOnly = true;
            check.FlatStyle = FlatStyle.Flat;
            check.DefaultCellStyle.ForeColor = Color.Gray;

   /*         DataGridViewButtonColumn ok = (DataGridViewButtonColumn)this.kiteDataView.Columns["Details"];
            ok.ReadOnly = true;
            ok.FlatStyle = FlatStyle.Flat;
            ok.DefaultCellStyle.ForeColor = Color.Gray;
            ok.DefaultCellStyle.BackColor = Color.White;
  //          ok.DefaultCellStyle.BackColor = Color.DarkGray;

            foreach (DataGridViewRow row in this.kiteDataView.Rows)
            {
//                (DataGridViewDisableCheckBoxCell)row.Cells["Fly"]
//                (DataGridViewDisableButtonCell)row.Cells["Details"]
//                row.Cells["Fly"].ReadOnly = true;
//                row.Cells["Fly"].Style.ForeColor = Color.Gray;

//              row.Cells["Details"].Style.ForeColor = Color.White;
//              row.Cells["Fly"].ReadOnly = true;
 //               row.
                row.Cells["Details"].ReadOnly = true;
            } */
        }

        public void RemoveKites()
        {
            foreach(DataGridViewRow row in this.kiteDataView.Rows)
            {
                DataGridViewCheckBoxCell check = (DataGridViewCheckBoxCell)row.Cells["Fly"];
                if(check.Value == check.TrueValue)
                {
                    this.kiteDataView.Rows.Remove(row);
                }
            }

/*            foreach (string kite in kiteNames)
            {
                foreach (DataGridViewRow row in this.kiteDataView.Rows)
                {
                    if (row.Cells["Domain"].Value.ToString().Equals(kite))
                    {
                        this.kiteDataView.Rows.Remove(row);
                    }
                }
            } */
        }

        public void UpdateStatus(bool flying, Dictionary<string, PkKite> kites)
        {
            foreach(DataGridViewRow row in this.kiteDataView.Rows)
            {
                string key = this.kiteDataView.Rows[row.Index].Cells["Domain"].Value.ToString();

                if(kites[key].Fly && flying)
                {
                    this.kiteDataView.Rows[row.Index].Cells["Status"].Value = "Flying";
                }
                else
                {
                    this.kiteDataView.Rows[row.Index].Cells["Status"].Value = "Grounded";
                }
            }
        }

        public bool GetChecked(Dictionary<string, PkKite> kites)
        {
            bool kitesSelected = false;

            foreach (DataGridViewRow row in this.kiteDataView.Rows)
            {
                DataGridViewCheckBoxCell check = (DataGridViewCheckBoxCell)row.Cells["Fly"];
                string key = this.kiteDataView.Rows[row.Index].Cells["Domain"].Value.ToString();
                if(check.Value == check.TrueValue)
                {
                    kitesSelected = true;
                    kites[key].Fly = true;
                }
                else
                {
                    kites[key].Fly = false;
                }
            }

            return kitesSelected;
        }

        private void kiteDataView_CellClick(object sender, DataGridViewCellEventArgs e)
        {
            if (e.RowIndex < 0 || e.ColumnIndex != this.kiteDataView.Columns["Details"].Index) return;
   
            string key = this.kiteDataView.Rows[e.RowIndex].Cells["Domain"].Value.ToString();
            PkKite kite = new PkKite();
            kite.Domain = key;

            PkKiteUpdateEventArgs args = new PkKiteUpdateEventArgs("details", kite);
            this.OnDetails_Click(args);
        }

        private void OnDetails_Click(PkKiteUpdateEventArgs e)
        {
            PkKiteUpdateHandler handler = this.KiteDetail_Click;
            
            if (handler != null)
            {
                handler(this, e);
            }
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