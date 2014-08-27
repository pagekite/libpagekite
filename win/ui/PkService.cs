using System;
using System.Collections;
using System.Collections.Generic;
using CookComputing.XmlRpc;

namespace Pagekite
{
    public class PkServiceInfo
    {
        public string AccountId { get; set; }

        public string Credentials { get; set; }

        public string DaysLeft { get; set; }

        public string MbLeft { get; set; }

        public string Secret { get; set; }
    }

    public static class PkService
    {
        public static PkServiceInfo Login(string a, string c, bool auto)
        {
            object[] val;
            IPkService proxy = XmlRpcProxyGen.Create<IPkService>();

            try
            {
                if (auto)
                {
                    val = proxy.login(a, c, c);
                }
                else
                {
                    val = proxy.login(a, c, "");
                }
            }
            catch(Exception e)
            {
                PkLogging.Logger(PkLogging.Level.Error, "Unable to connect to the Pagekite Service"
                                 + Environment.NewLine + "Exception: " + e.Message, true);
                val = null;
            }

            if (val == null)
            {
                return null;
            }

            string ok = val[0].ToString();

            if (!ok.Equals("ok"))
            {
                if (auto)
                {
                    return null;
                }
                else
                {
                    PkLogging.Logger(PkLogging.Level.Warning, "Incorrect information", true);
                    return null;
                }
            }

            string[] data = (string[])val[1];

            PkServiceInfo info = new PkServiceInfo();

            info.AccountId = data[0];
            info.Credentials = data[1];

            return info;
        }

        public static bool Logout(string a, string c)
        {
            return false;
        }

        public static bool CreateAccount(string email, string kitename)
        {
            object[] val;
            IPkService proxy = XmlRpcProxyGen.Create<IPkService>();

            try
            {
                val = proxy.create_account("", "", email, kitename, true, true, false);
            }
            catch (Exception e)
            {
                PkLogging.Logger(PkLogging.Level.Error, "Unable to connect to the Pagekite Service"
                                 + Environment.NewLine + "Exception: " + e.Message, true);
                val = null;
            }

            if (val == null)
            {
                return false;
            }

            string ok = val[0].ToString();

            if (!ok.Equals("ok"))
            {
                switch(ok)
                {
                    case "email":
                        PkLogging.Logger(PkLogging.Level.Warning, "Invalid email address", true);
                        break;
                    case "subdomain":
                        PkLogging.Logger(PkLogging.Level.Warning, "Invalid subdomain", true);
                        break;
                    case "domain":
                        PkLogging.Logger(PkLogging.Level.Warning, "Invalid domain", true);
                        break;
                    case "pleaselogin":
                        PkLogging.Logger(PkLogging.Level.Warning, "Please log in", true);
                        break;
                    case "domaintaken":
                        PkLogging.Logger(PkLogging.Level.Warning, "This domain is taken", true);
                        break;
                    default:
                        PkLogging.Logger(PkLogging.Level.Warning, "Invalid information" + Environment.NewLine 
                                         + ok, true);
                        break;
                }

                return false;
            }

            return true;
        }

        public static PkData GetAccountInfo(string accountId, string credentials)
        {
            object[] val;
            IPkService proxy = XmlRpcProxyGen.Create<IPkService>();

            try
            {
                val = proxy.get_account_info(accountId, credentials, "");
            }
            catch (Exception e)
            {
                PkLogging.Logger(PkLogging.Level.Error, "Unable to connect to the Pagekite Service"
                                 + Environment.NewLine + "Exception: " + e.Message, true);
                val = null;
            }

            if(val == null)
            {
                return null;
            }

            string ok = val[0].ToString();

            if(!ok.Equals("ok"))
            {
                PkLogging.Logger(PkLogging.Level.Error, "Unable to update account information");
                return null;
            }

            XmlRpcStruct serviceStruct = (XmlRpcStruct)val[1];

            PkData data = new PkData();

            XmlRpcStruct serviceData = (XmlRpcStruct)serviceStruct["data"];

            data.ServiceInfo.Secret = serviceData["_ss"].ToString();

            data.ServiceInfo.DaysLeft = serviceData["days_left"].ToString();

            data.ServiceInfo.MbLeft = serviceData["quota_mb_left"].ToString();

            object[] serviceKites = (object[])serviceData["kites"];

            foreach (XmlRpcStruct serviceKite in serviceKites)
            {
                PkKite kite = new PkKite();
                kite.Domain = serviceKite["domain"].ToString();
                kite.Secret = serviceKite["ssecret"].ToString();
                data.Kites.Add(kite.Domain, kite);
            }
        
            return data;
        }

        public static Dictionary<string, string[]> GetAvailableDomains(string accountId, string credentials)
        {
            object[] val;
            IPkService proxy = XmlRpcProxyGen.Create<IPkService>();

            try
            {
                val = proxy.get_available_domains(accountId, credentials);
            }
            catch (Exception e)
            {
                PkLogging.Logger(PkLogging.Level.Error, "Unable to connect to the Pagekite Service"
                                 + Environment.NewLine + "Exception: " + e.Message, true);
                val = null;
            }

            if (val == null)
            {
                return null;
            }

            string ok = val[0].ToString();

            if (!ok.Equals("ok"))
            {
                PkLogging.Logger(PkLogging.Level.Error, "Unable to fetch available domains"
                                 + Environment.NewLine + ok, true);
                return null;
            }

            XmlRpcStruct data = (XmlRpcStruct)val[1];

            Dictionary<string, string[]> domains = new Dictionary<string, string[]>();

            foreach (string key in data.Keys)
            {
                domains.Add(key, (string[])data[key]);
            }

            return domains;
        }

        public static Dictionary<string, double> GetKiteStats(string accountId, string credentials)
        {
            object[] val;
            IPkService proxy = XmlRpcProxyGen.Create<IPkService>();

            try
            {
                val = proxy.get_kite_stats(accountId, credentials);
            }
            catch (Exception e)
            {
                PkLogging.Logger(PkLogging.Level.Error, "Unable to connect to the Pagekite Service"
                                 + Environment.NewLine + "Exception: " + e.Message, true);
                val = null;
            }

            if (val == null)
            {
                return null;
            }

            string ok = val[0].ToString();

            if(!ok.Equals("ok"))
            {
                PkLogging.Logger(PkLogging.Level.Error, "Unable to update kites");
                return null;
            }

            XmlRpcStruct kitesStruct = (XmlRpcStruct)val[1];

            Dictionary<string, double> kites = new Dictionary<string, double>();

            foreach(string kite in kitesStruct.Keys)
            {
                try
                {
                    kites[kite] = Convert.ToDouble(kitesStruct[kite]);
                }
                catch(Exception e)
                {
                    kites[kite] = -1;
                }
            }

            return kites;
        }

        public static bool AddKite(string accountId, string credentials, string kitename)
        {
            object[] val;
            IPkService proxy = XmlRpcProxyGen.Create<IPkService>();

            try
            {
                val = proxy.add_kite(accountId, credentials, kitename, false);
            }
            catch (Exception e)
            {
                PkLogging.Logger(PkLogging.Level.Error, "Unable to connect to the Pagekite Service"
                                 + Environment.NewLine + "Exception: " + e.Message, true);
                val = null;
            }

            if (val == null)
            {
                return false;
            }

            string ok = val[0].ToString();

            if (!ok.Equals("ok"))
            {
                PkLogging.Logger(PkLogging.Level.Error, "Unable to add kite" 
                                 + Environment.NewLine + ok, true);
                return false;
            }

            PkLogging.Logger(PkLogging.Level.Info, kitename + " added.");

            return true;

        }

        public static bool DeleteKites(string accountId, string credentials, List<string> kitenames)
        {
            object[] val;
            IPkService proxy = XmlRpcProxyGen.Create<IPkService>();

            string[] kites = new string[kitenames.Count];
            for (int i = 0; i < kites.Length; i++)
            {
                kites[i] = kitenames[i];
            }

            try
            {
                val = proxy.delete_kites(accountId, credentials, kites);
            }
            catch (Exception e)
            {
                PkLogging.Logger(PkLogging.Level.Error, "Unable to connect to the Pagekite Service"
                                 + Environment.NewLine + "Exception: " + e.Message, true);
                val = null;
            }

            if (val == null)
            {
                return false;
            }

            string ok = val[0].ToString();

            if (!ok.Equals("ok"))
            {
                PkLogging.Logger(PkLogging.Level.Error, "Unable to remove kites", true);
                return false;
            }

            PkLogging.Logger(PkLogging.Level.Info, kitenames.Count + " kites removed.");

            return true;
        }
    }

    [XmlRpcUrl("https://pagekite.net/xmlrpc/")]
    public interface IPkService : IXmlRpcProxy
    {
        [XmlRpcMethod]
        object[] login(string a, string c, string credential);

        [XmlRpcMethod]
        object[] logout(string a, string c, bool revoke_all);

        [XmlRpcMethod]
        object[] create_account(string a, string c, string email, string kitename,
            bool accept_terms, bool send_mail, bool activate);

        [XmlRpcMethod]
        object[] get_account_info(string a, string c, string version);

        [XmlRpcMethod]
        object[] get_available_domains(string a, string c);

        [XmlRpcMethod]
        object[] get_kite_stats(string a, string c);

        [XmlRpcMethod]
        object[] add_kite(string a, string c, string kitename, bool check_cnames);

        [XmlRpcMethod]
        object[] delete_kites(string a, string c, string[] kitenames);
    }
}