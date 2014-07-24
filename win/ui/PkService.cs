using System;
using System.Collections.Generic;
using CookComputing.XmlRpc;

namespace Pagekite
{
    public struct PkServiceValue
    {
        public string ok;
        public List<string> value;
    }

    [XmlRpcUrl("https://pagekite.net/xmlrpc/")]
    public interface IPkService : IXmlRpcProxy
    {
        [XmlRpcMethod]
        string[][] login(string a, string c, string credential);

        [XmlRpcMethod]
        string[][] logout(string a, string c, bool revoke_all);

        [XmlRpcMethod]
        string[][] create_account(string a, string c, string email, string kitename,
            bool accept_terms, bool send_mail, bool activate);

        [XmlRpcMethod]
        string[][] get_account_info(string a, string c, string version);

        [XmlRpcMethod]
        string[][] get_available_domains(string a, string c);

        [XmlRpcMethod]
        string[][] delete_kites(string a, string c, string[] kitenames);
    }
}