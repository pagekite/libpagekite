using System;

namespace Pagekite
{
    public class PkKiteUpdateEventArgs : EventArgs
    {
        private PkKite pKite;
        private string pEvent;

        public PkKiteUpdateEventArgs(string Event, PkKite kite)
        {
            this.pEvent = Event;
            this.pKite = kite;
        }

        public PkKite Kite
        {
            get
            {
                return pKite;
            }
        }

        public string Event
        {
            get
            {
                return pEvent;
            }
        }
    }
}