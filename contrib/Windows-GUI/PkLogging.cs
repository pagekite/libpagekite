using System;
using System.Drawing;
using System.Windows.Forms;

namespace Pagekite
{
    public static class PkLogging
    {
        public static event EventHandler LogPanelUpdate = delegate {};

        private const int LOG_MAX_LENGTH = 64 * 1024;
        private static int libLogLines = 0;

        public static string Log = "";
        public static bool Debug = false;

        public enum Level
        {
            Lib,
            Info,
            Debug,
            Warning,
            Error,
        }

        public static void Logger(Level level, string message, bool dialog = false)
        {
            switch(level)
            {
                case Level.Lib:
                    UpdateLogLib(message);
                    break;

                case Level.Info:
                    if (dialog)
                    {
                        MessageBox.Show(message, "Information", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        return;
                    }
                    UpdateLog("[Main] " + message + Environment.NewLine);
                    break;

                case Level.Debug:
                    if (!PkLogging.Debug)
                    {
                        return;
                    }
                    UpdateLog("[Debug] " + message + Environment.NewLine);
                    break;

                case Level.Warning:
                    if (dialog)
                    {
                        MessageBox.Show(message, "Warning", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        return;
                    }
                    UpdateLog("[Warning] " + message + Environment.NewLine);
                    break;

                case Level.Error:
                    if (dialog)
                    {
                        MessageBox.Show(message , "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                    UpdateLog("[Error] " + message + Environment.NewLine);
                    break;
            }

            LogPanelUpdate(null, EventArgs.Empty);
        }

        public static void GroundLib()
        {
            libLogLines = 0;
        }

        private static void UpdateLog(string message)
        {
            Log += message;
            
            if(Log.Length > LOG_MAX_LENGTH)
            {
                int len = Log.Length - LOG_MAX_LENGTH - 5000; //fixme maybe?

                Log = Log.Substring(len, LOG_MAX_LENGTH);
            }
        }

        private static void UpdateLogLib(string message)
        {
            if (message == null)
            {
                return;
            }

            int i = 0;
            int j = 0;
            int count = 0;

            while((i = message.IndexOf("\n", i)) != -1)
            {
                if(count < libLogLines)
                {
                    j = i;
                }

                count++;
                i++;
            }

            if (count != libLogLines)
            {
                message = message.Substring(j, message.Length - j);
                message = message.Replace("\n", Environment.NewLine + "[Lib] ");
                message = message.Substring(0, message.Length - 7);

                if (libLogLines == 0)
                {
                    message = "[Lib] " + message;
                }

                libLogLines = count;

                Log += message;
            }
        }
    }
}