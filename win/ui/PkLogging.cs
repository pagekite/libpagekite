using System;
using System.Drawing;
using System.Windows.Forms;

namespace Pagekite
{
    public static class PkLogging
    {
        public static event EventHandler LogPanelUpdate = delegate {};

        private const int LOG_MAX_LENGTH = 64 * 1024;
        private static string logstr = "";

        public enum Level
        {
            Lib,
            Debug,
            Warning,
            Error,
        }

        public static void Log(Level level, string message, bool dialog = false)
        {
            switch(level)
            {
                case Level.Lib:
                    UpdateLog("[lib] " + message);
                    break;

                case Level.Debug:
                    UpdateLog("[main] " + message);
                    break;

                case Level.Warning:
                    if (dialog)
                    {
                        MessageBox.Show(message, "Warning");
                    }
                    UpdateLog("[Warning] " + message);
                    break;

                case Level.Error:
                    if (dialog)
                    {
                        MessageBox.Show(message, "Error");
                    }
                    UpdateLog("[Error] " + message);
                    break;
            }
        }

        public static string GetLog()
        {
            return logstr;
        }

        private static void UpdateLog(string message)
        {
            logstr += message;
            
            if(logstr.Length > LOG_MAX_LENGTH)
            {
                int len = logstr.Length - LOG_MAX_LENGTH - 5000;

                logstr = logstr.Substring(len, LOG_MAX_LENGTH);
            }

            LogPanelUpdate(null, EventArgs.Empty);
        }
    }
}