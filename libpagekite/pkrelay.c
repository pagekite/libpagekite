/******************************************************************************
pkrelay.c - Logic specific to front-end relays

*******************************************************************************

   (:   AGPL! AGPL! AGPL! AGPL!   Not Apache!   AGPL! AGPL! AGPL! AGPL!  :)

*******************************************************************************

This file is Copyright 2011-2017, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify it under
the terms of the  GNU Affero General Public License, version 3.0 or above, as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the Apache License for more details.

You should have received a copy of the GNU AGPL along with this program.
If not, see: <http://www.gnu.org/licenses/agpl.html>

Note: For alternate license terms, see the file COPYING.md.

*******************************************************************************

The PageKite tunneling protocol is completely symmetric, so once a
connection has been identified (and possibly authenticated), the same event
loop as handles connectors will work for relays.

Connections are accepted and identified like so:

  - Listeners added to main event-loop: pkr_conn_accepted_cb
  - A be_conn is allocated for each incoming connection
  - Event-loop watches for incoming data: pkr_new_conn_readable_cb
  - A "peek" is used to try recognize protocols
  - Once a protocol is identified, a few things may happen:
     - The be_conn may be wrapped with TLS/SSL
     - The be_conn may be discarded and upgraded to a tunnel
     - The be_conn may be assigned as a stream to a particular tunnel

******************************************************************************/

#ifndef __GNUC__
#error Sorry, PageKite relay support requires the GNU C __thread extension.
#endif

#include "pagekite.h"

#define _GNU_SOURCE
#include <ctype.h>
#include <string.h>
#include <time.h>

#include "pkcommon.h"
#include "pkutils.h"
#include "pkstate.h"
#include "pkerror.h"
#include "pkhooks.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pktunnel.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pkrelay.h"
#include "pklogging.h"

#define PARSE_FAILED         -2
#define PARSE_WANT_MORE_DATA -1
#define PARSE_UNDECIDED       0
#define PARSE_MATCH_FAST      1
#define PARSE_MATCH_SLOW      2

static const char* known_protos[PKR_PROTO_MAX] = {
  "(none)",
  "PageKite",
  "HTTP",
  "HTTP CONNECT",
  "RAW TCP/IP",
  "TLS",
  "Legacy SSL",
  "PageKite PING",
  "(any)"};

static const char* proto_protonames[PKR_PROTO_MAX] = {
  "none",
  "pagekite",
  "http",
  "raw",
  "raw",
  "https",
  "https",
  "pagekite-ping",
  "any"};

#define PEEK_BYTES (16 * 1024)  /* Some HTTP headers are large! */
#define SSL_CLIENTHELLO 0x80
#define TLS_CLIENTHELLO 0x16

struct incoming_conn_state {
  int peeked_bytes;
  char peeked[PEEK_BYTES+1];
#ifdef HAVE_IPV6
  /* This should be strictly larger than sockaddr_in,
   * so the data will fit either way. Is this a hack? */
  struct sockaddr_in6 local_addr;
  struct sockaddr_in6 remote_addr;
#else
  struct sockaddr_in local_addr;
  struct sockaddr_in remote_addr;
#endif
  struct pk_manager* pkm;
  struct pk_backend_conn* pkb;
  struct pk_tunnel* tunnel;
  char* hostname;
  int parsed_as;
  int port;
  pkr_proto_enum proto;
  int parse_state;
  time_t created;
};



/*****************************************************************************
 * The data structure of our kite index:
 *
 *   pkr->kite_index = A sorted listed of kite info (char **) objects.
 *  
 * Each kite info object consists of TWO zero-terminated arrays, one after
 * the other. The first is an array of chars (a C string) representing the
 * kite name. The second array is a list of porto/port/tunnel mappings, where
 * an entry with (proto == PKR_PROTO_NONE) is the end-of-list marker.
 *
 *  char kitename[];
 *  struct pkr_port_proto_pointer[];
 *
 * These are malloced together and organized in such a way that searching
 * and cleanup can be done using standard C functions (strcasecmp, free).
 ****************************************************************************/

static void _pkr_kite_index_read_start(struct pk_relay_manager* pkrm) {
  /* If we had a way to ensure the ++ and -- ops were atomic,
   * we might not need so many locks. Le sigh! */
  pthread_mutex_lock(&(pkrm->kite_index_readblock));
  pthread_mutex_lock(&(pkrm->kite_index_readlock));
  pkrm->kite_index_readers++;
  pthread_mutex_unlock(&(pkrm->kite_index_readlock));
  pthread_mutex_unlock(&(pkrm->kite_index_readblock));
}

static void _pkr_kite_index_read_stop(struct pk_relay_manager* pkrm) {
  pthread_mutex_lock(&(pkrm->kite_index_readlock));
  pkrm->kite_index_readers--;
  pthread_mutex_unlock(&(pkrm->kite_index_readlock));
}

static size_t _pkr_first_ppp_offset(const char* kite_info)
{
  size_t ppps = sizeof(struct pkr_port_proto_pointer);
  return ((strlen(kite_info) / ppps) + 1) * ppps;
}

static struct pkr_port_proto_pointer* _pkr_first_ppp(const char* kite_info)
{
  return ((struct pkr_port_proto_pointer*)
          (kite_info + _pkr_first_ppp_offset(kite_info)));
}

static struct pkr_port_proto_pointer* _pkr_find_ppp(
  const char* kite_info,
  struct incoming_conn_state* ics)
{
  if (kite_info == NULL) return NULL;
  struct pkr_port_proto_pointer* ppp = _pkr_first_ppp(kite_info);
  while (ppp->proto != PKR_PROTO_NONE) {
    if (((ppp->proto == PKR_PROTO_ANY) || (ppp->proto == ics->proto)) &&
        ((ppp->port < 1)               || (ppp->port == ics->port))) {
      return ppp;
    }
    ppp++;
  }
  return NULL;
}

static char* _pkr_find_kite_info(struct pk_manager* pkm,
                                 const char* kitename)
{
  struct pk_relay_manager* pkrm = PKRM(pkm);
  if (pkrm->flying_kites < 1) return NULL;

  int slot = strcaseindex(pkrm->kite_index, kitename, pkrm->flying_kites);
  char** kite_info = pkrm->kite_index + slot;
  if (0 == strcasecmp(kitename, *kite_info)) {
    return *kite_info;
  }
  return NULL;
}

static size_t _pkr_kilen(char *kite_info) {
  size_t kilen = _pkr_first_ppp_offset(kite_info);
  struct pkr_port_proto_pointer* ppp;
  ppp = (struct pkr_port_proto_pointer*) (kite_info + kilen);

  kilen += sizeof(struct pkr_port_proto_pointer);
  while (ppp->proto != PKR_PROTO_NONE) {
    kilen += sizeof(struct pkr_port_proto_pointer);
    ppp++;
  }

  return kilen;
}

pkr_proto_enum _pkr_parse_proto(const char* protocol)
{
  if (0 == strcasecmp(protocol, "https")) return PKR_PROTO_TLS;
  if (0 == strcasecmp(protocol, "http")) return PKR_PROTO_HTTP;
  if (0 == strcasecmp(protocol, "raw")) return PKR_PROTO_RAW;
  return PKR_PROTO_NONE;
}

static char* _pkr_add_kite_info(struct pk_manager* pkm,
                                const char* kitename,
                                pkr_proto_enum proto,
                                int port,
                                struct pk_tunnel* tunnel)
{
  struct pk_relay_manager* pkrm = PKRM(pkm);

  /* We lock the whole function; we don't want anyone else changing
   * the size of the index out from under us. There are also fine-grained
   * locks on critical sections to protect reader integrity. */
  pthread_mutex_lock(&(pkrm->kite_index_lock));

  struct pkr_port_proto_pointer* ppp0;
  struct pkr_port_proto_pointer* ppp;
  size_t kilen;
  size_t ppp_s = sizeof(struct pkr_port_proto_pointer);
  char **old_kite_index = NULL;
  char **new_kite_index = NULL;
  char *new_kite_info = NULL;
  char *kite_info = NULL;
  int flying_kites = pkrm->flying_kites;
  int slot;

  PK_TRACE_FUNCTION;

  int first_ppp_offset = _pkr_first_ppp_offset(kitename);
  if (pkrm->flying_kites < 1 || !pkrm->kite_index) {
    slot = 0;
    flying_kites = 1;
    new_kite_index = malloc(sizeof(char**));
    kilen = first_ppp_offset + ppp_s;
  }
  else {
    slot = strcaseindex(pkrm->kite_index, kitename, pkrm->flying_kites);
    kite_info = *(pkrm->kite_index + slot);
    if ((slot < flying_kites) &&
        (0 == strcasecmp(kite_info, kitename))) {
      flying_kites = pkrm->flying_kites;
      new_kite_index = pkrm->kite_index;
      kilen = _pkr_kilen(kite_info);
    }
    else {
      /* Inserting new entry, copy and resize index */
      flying_kites = pkrm->flying_kites + 1;
      old_kite_index = pkrm->kite_index;
      new_kite_index = malloc(sizeof(char**) * flying_kites);
      if (new_kite_index != NULL) {
        if (slot) memcpy(new_kite_index, old_kite_index, slot * sizeof(char**));
        memcpy(new_kite_index + slot + 1,
               old_kite_index + slot,
               (flying_kites - slot - 1) * sizeof(char**));
        /* New entry: reserve space for the kitename and end-of-list marker. */
        kilen = first_ppp_offset + ppp_s;
        kite_info = NULL;
      }
    }
  }

  /* Did the mallocs above fail? */
  if (new_kite_index == NULL) {
    pthread_mutex_unlock(&(pkrm->kite_index_lock));
    return NULL;
  }

  /* Reserve space for existing data + new PPP entry */
  new_kite_info = malloc(kilen + ppp_s);
  if (new_kite_info == NULL) {
    pthread_mutex_unlock(&(pkrm->kite_index_lock));
    return NULL;
  }

  memset(new_kite_info, 0, kilen + ppp_s);
  if (kite_info) {
    memcpy(new_kite_info, kite_info, kilen);
  }
  else {
    strcpy(new_kite_info, kitename);
  }

  /* Bubble-sort our new data into place. Lame, but KISS? */
  ppp0 = (struct pkr_port_proto_pointer*) (new_kite_info + first_ppp_offset);
  ppp = (struct pkr_port_proto_pointer*) (new_kite_info + kilen);
  ppp--;
  while (ppp > ppp0) {
    if ((proto > (ppp-1)->proto) ||
        ((proto == (ppp-1)->proto) && (port < (ppp-1)->port))) break;
    ppp->tunnel = (ppp-1)->tunnel;
    ppp->proto = (ppp-1)->proto;
    ppp->port = (ppp-1)->port;
    ppp--;
  }
  ppp->port = port;
  ppp->tunnel = tunnel;
  ppp->proto = proto;

  /* Enter new kite info into index. This may be visible to other
   * threads, in the case where the index size stayed unchanged, but
   * as the change is atomic, that should be fine. */
  *(new_kite_index + slot) = new_kite_info;

  if (kite_info || old_kite_index || (new_kite_index != pkrm->kite_index)) {
    /* Make sure nobody is reading before making dangerous changes... */
    pthread_mutex_lock(&(pkrm->kite_index_readblock));
    while (pkrm->kite_index_readers) pthread_yield();

    /* Swap in our new index, if we actually created a new one. */
    if (new_kite_index != pkrm->kite_index) {
      pkrm->kite_index = new_kite_index;
      pkrm->flying_kites = flying_kites;
    }

    /* Release old buffers */
    if (kite_info) free(kite_info);
    if (old_kite_index) free(old_kite_index);

    /* Unleash the readers... */
    pthread_mutex_unlock(&(pkrm->kite_index_readblock));
  }

  /* All done! */
  pthread_mutex_unlock(&(pkrm->kite_index_lock));
  return new_kite_info;
}

static char* _pkr_remove_kite_info(struct pk_manager* pkm,
                                   const char* kitename,
                                   struct pk_tunnel* tunnel)
{
  struct pk_relay_manager* pkrm = PKRM(pkm);

  /* We lock the whole function; we don't want anyone else changing
   * the size of the index out from under us. There are also fine-grained
   * locks on critical sections to protect reader integrity. */
  pthread_mutex_lock(&(pkrm->kite_index_lock));
  PK_TRACE_FUNCTION;

  /* Make sure nobody is reading before making dangerous changes... */
  pthread_mutex_lock(&(pkrm->kite_index_readblock));
  while (pkrm->kite_index_readers) pthread_yield();

  /* Find the kite info */
  int slot = strcaseindex(pkrm->kite_index, kitename, pkrm->flying_kites);
  char* new_kite_info = *(pkrm->kite_index + slot);
  if ((slot < pkrm->flying_kites) && 
      (0 == strcasecmp(new_kite_info, kitename))) {
    int first_ppp_offset = _pkr_first_ppp_offset(new_kite_info);

    /* Walk the list of PPP entries, deleting any that mention the tunnel
     * we are shutting down. */
    struct pkr_port_proto_pointer* ppp0;
    ppp0 = (struct pkr_port_proto_pointer*) (new_kite_info + first_ppp_offset);
    struct pkr_port_proto_pointer* ppps = ppp0;
    struct pkr_port_proto_pointer* pppd = ppp0;
    while (ppps->proto != PKR_PROTO_NONE) {
      /* Copy source to destination, if they differ and not removing. */
      if ((ppps != pppd) && (ppps->tunnel != tunnel)) {
        pppd->tunnel = ppps->tunnel;
        pppd->proto = ppps->proto;
        pppd->port = ppps->port;
      }
      /* Clear source, if copied or we are removing. */
      if ((ppps != pppd) || (ppps->tunnel == tunnel)) {
        ppps->tunnel = NULL;
        ppps->proto = PKR_PROTO_NONE;
        ppps->port = -1;
      }
      /* Move destination forward if current destination is not clear. */
      if (pppd->proto != PKR_PROTO_NONE) pppd++;
      /* Move source forward */
      ppps++;
    }
    if (ppp0->proto == PKR_PROTO_NONE) {
      /* FIXME: Should we compact the index? Otherwise RAM usage grows without
       *        bounds. But on the other hand, we know that disconnected kites
       * are quite likely to be reconnected again very soon, so by leaving the
       * entry here we can save ourselves a bit of work. Most efficient is
       * probably to leave intact, but garbage collect now and then.
       * (The simplest garbage collection would be to restart the app...)
       */
    }
  }

  /* Unleash the readers... */
  pthread_mutex_unlock(&(pkrm->kite_index_readblock));

  /* All done! */
  pthread_mutex_unlock(&(pkrm->kite_index_lock));
  return new_kite_info;
}

static struct pk_tunnel* pkr_find_tunnel(struct incoming_conn_state* ics)
{
  struct pk_relay_manager* pkrm = PKRM(ics->pkm);
  struct pkr_port_proto_pointer* ppp;
  char* kite_info;

  PK_TRACE_FUNCTION;
  _pkr_kite_index_read_start(pkrm);

  /* First: search based on the hostname */
  if ((ics->hostname) &&
      (NULL != (kite_info = _pkr_find_kite_info(ics->pkm, ics->hostname))) &&
      (NULL != (ppp = _pkr_find_ppp(kite_info, ics)))) {
    ics->tunnel = ppp->tunnel;
    _pkr_kite_index_read_stop(pkrm);
    return ics->tunnel;
  }

#ifdef HAVE_IPV6
  /* Second: search based on the IPv6 address */
  char ipaddr[120];
  in_ipaddr_to_str((struct sockaddr*) &(ics->local_addr), ipaddr, 120);
  if ((NULL != (kite_info = _pkr_find_kite_info(ics->pkm, ipaddr))) &&
      (NULL != (ppp = _pkr_find_ppp(kite_info, ics)))) {
    ics->tunnel = ppp->tunnel;
    _pkr_kite_index_read_stop(pkrm);
    return ics->tunnel;
  }
#endif

  _pkr_kite_index_read_stop(pkrm);
  return NULL;
}

static char* TLS_PARSE_FAILED = "TLS parse failed";
static char* _pkr_get_first_tls_sni(unsigned char* peeked, int bytes)
{
  /* FIXME: This should probably be refactored to be more clear. As it is,
   *        this is just a direct port of the logic from pagekite.py.
   *        If I made any errors in the bounds checking, then this routine
   *        may become a security vulnerability.
   *
   * WARNING: This function is destructive, it will insert a null marker
   *          after the first thing it thinks is an SNI name.
   */
  int pos = 5;
  while ((pos + 4) < bytes)
  {
    unsigned char* msg = &(peeked[pos]);
    unsigned char mtype = msg[0];
    int mlen = (msg[1] << 16) + (msg[2] << 8) + msg[3];
    pos += 4;
    if ((mtype == 1) && (pos + mlen <= bytes))
    {
      /* We have a complete ClientHello! We only care about the extensions,
       * so we blindly skip over all the boring bits. Yay magic numbers!
       */
      unsigned char* ch = &(peeked[pos]);
      int slen = ch[34];
      int cslen = (ch[35 + slen] << 8) + ch[36 + slen];
      int cmlen = ch[37 + slen + cslen];
      int skip = (34 + 2 +                           /* 34, 2 = Magic :( */
                  1 + slen + 2 + cslen + 1 + cmlen);
      while (skip + 4 < mlen)                        /* 4 = Header */
      {
        /* OK! We have extensions... look at them.
         */
        unsigned char* che = ch + skip;
        int etype = (che[0] << 8) + che[1];
        int elen = (che[2] << 8) + che[3];
        skip += 4;
        if ((etype == 0) &&                          /* 0 = SNI */
            (elen > 5) &&                            /* 5 = Magic + Header */
            (skip + elen < mlen))                    /* Bounds check */
        {
          /* OK, this is an SNI name list. Do we find a name?
           */
          skip += 2;                                 /* 2 = Magic :( */
          int ntype = ch[skip];
          int nlen = (ch[skip + 1] << 8) + ch[skip + 2];
          skip += 3;                                 /* 3 = size of header */
          if ((ntype == 0) &&                        /* 0 = SNI name */
              (skip + nlen < mlen))                  /* Bounds check */
          {
            /* Extract and return the 1st name as a C string
             */
            ch[skip + nlen] = '\0';                  /* Destruction! */
            return (char*) &ch[skip];
          }
          else skip += nlen;
        }
        else skip += elen;
      }
    }
    pos += mlen;
  }
  if (pos < bytes) return TLS_PARSE_FAILED;
  return NULL;
}

static int _pkr_parse_ssltls(unsigned char* peeked,
                             int bytes,
                             struct incoming_conn_state* ics)
{
  /* Is this TLS? */
  if (peeked[0] == TLS_CLIENTHELLO) {
    char* tls_sni = _pkr_get_first_tls_sni(peeked, bytes);
    if (tls_sni == TLS_PARSE_FAILED) return PARSE_WANT_MORE_DATA;
    ics->hostname = tls_sni;
    ics->proto = PKR_PROTO_TLS;
    return pkr_find_tunnel(ics) ? PARSE_MATCH_FAST : PARSE_MATCH_SLOW;
  }
  else if (peeked[0] == SSL_CLIENTHELLO) {
    ics->proto = PKR_PROTO_LEGACY_SSL;
    return pkr_find_tunnel(ics) ? PARSE_MATCH_FAST : PARSE_MATCH_SLOW;
  }

  /* -Wall dislikes unused arguments */
  (void) bytes;
  return PARSE_UNDECIDED;
}


#define IS_PAGEKITE_REQUEST(p) (strncasecmp(p, PK_HANDSHAKE_CONNECT, \
                                            strlen(PK_HANDSHAKE_CONNECT)-2 \
                                            ) == 0)
static int _pkr_pagekite_request_ready(char* peeked,
                                       int bytes,
                                       char* first_nl,
                                       struct incoming_conn_state* ics)
{
  char *end;
  /* Since we know pagekite requests are small and peekable, we
   * can check if it's complete and process if so, otherwise defer.
   */
  if ((*(first_nl-1) == '\r') ? (strstr(peeked, "\r\n\r\n") != NULL)
                              : (strstr(peeked, "\n\n") != NULL)) {
    ics->proto = PKR_PROTO_PAGEKITE;
    return PARSE_MATCH_SLOW;
  }
  return PARSE_WANT_MORE_DATA;
}


static int _pkr_parse_http(char* peeked,
                           int bytes,
                           struct incoming_conn_state* ics)
{
  /* This will assume HTTP if we find all of the following things:
   *   - The string " HTTP/" or " http/" on the first line of data
   *   - A space preceding the " HTTP/" marker
   *   - The first character is alphabetic, according to isalpha()
   *
   * We deliberately do NOT look for "get" or "post", because there
   * are so many other interesting HTTP verbs out there.
   */
  char* sp = strchr(peeked, ' ');
  if (sp == NULL) return PARSE_UNDECIDED;

  char* nl = strchr(peeked, '\n');
  if ((nl == NULL) || (nl < sp)) return PARSE_UNDECIDED;

  char* http = strcasestr(peeked, " HTTP/");
  if (http && (sp < http) && (nl > http) && isalpha(peeked[0])) {

    /* - Is it a PageKite PING? */
    if (strcasecmp(peeked, PK_FRONTEND_PING) == 0) {
      ics->proto = PKR_PROTO_PAGEKITE_PING;
      ics->hostname = NULL;
      return PARSE_MATCH_FAST;
    }

    /* - Is it a PageKite tunnel request? */
    if (IS_PAGEKITE_REQUEST(peeked)) {
      return _pkr_pagekite_request_ready(peeked, bytes, nl, ics);
    }

    /* - Is it an HTTP Proxy request? */
    if (strncasecmp(peeked, "CONNECT ", 8) == 0) {
      /* HTTP PROXY requests are one-liners. However, we need to consume
       * the data, so we don't parse until we have the full request. */
      char* crlf2 = strstr(peeked, "\r\n\r\n") + 4;
      if (crlf2 == (NULL + 4)) crlf2 = strstr(peeked, "\n\n") + 2;
      if (crlf2 < peeked) return PARSE_WANT_MORE_DATA;
      ics->peeked_bytes = (crlf2 - peeked);

      /* If it's valid, we start
       * tunneling, if it's not, we close. We complete the handshake
       * on another thread, just in case it blocks. */
      ics->hostname = peeked + 8;
      ics->proto = PKR_PROTO_HTTP_CONNECT;
      int hlen = zero_first_whitespace(bytes - 8, ics->hostname);
      hlen = zero_nth_char(1, ':', hlen, ics->hostname);
      if (hlen) sscanf(ics->hostname + hlen, "%d", &(ics->port));

      return pkr_find_tunnel(ics) ? PARSE_MATCH_SLOW : PARSE_FAILED;
    }

    /* The other HTTP implementations all prefer the Host: header */
    ics->proto = PKR_PROTO_HTTP;
    char* host = strcasestr(peeked, "\nHost: ");
    if (host) {
      host += 7;
      int hlen = zero_first_whitespace(bytes - (host - peeked), host);
      if (hlen) {
        ics->hostname = host;
        zero_nth_char(1, ':', hlen, ics->hostname);
      }
      else {
        host = NULL;
      }
    }

    if ((host == NULL)
             && ((*(nl-1) == '\r') ? (strstr(peeked, "\r\n\r\n") == NULL)
                                   : (strstr(peeked, "\n\n") == NULL))
             && (bytes < PEEK_BYTES)) {
      /* No Host: header, and request is incomplete... */
      return PARSE_WANT_MORE_DATA;
    }

    if (pkr_find_tunnel(ics)) return PARSE_MATCH_FAST;

    return PARSE_FAILED;
  }

  /* -Wall dislikes unused arguments */
  (void) bytes;
  return PARSE_UNDECIDED;
}

static void _pkr_consume_peeked(struct incoming_conn_state* ics)
{
  char buf[PK_RESPONSE_MAXSIZE];
  /* Clear the OS buffers */
  while (ics->peeked_bytes > 0) {
    int rb = PKS_read(ics->pkb->conn.sockfd, buf, PK_RESPONSE_MAXSIZE);
    if (rb > 0) ics->peeked_bytes -= rb;
  }
}

static void _pkr_close(struct incoming_conn_state* ics,
                       int log_level,
                       const char* reason)
{
  char rbuf[128];
  char lbuf[128];

  /* FIXME: Log more useful things about the connection. */
  if (reason != NULL) {
    pk_log(PK_LOG_TUNNEL_CONNS | log_level,
           "%s; remote=%s; local=%s; hostname=%s; proto=%s; port=%d; fd=%d",
           reason,
           in_addr_to_str((struct sockaddr*) &(ics->remote_addr), rbuf, 128),
           in_addr_to_str((struct sockaddr*) &(ics->local_addr), lbuf, 128),
           ics->hostname,
           known_protos[ics->proto],
           ics->port,
           ics->pkb->conn.sockfd);
  }

  if (ics->pkb) {
    if (ics->proto == PKR_PROTO_PAGEKITE) {
      /* no-op: the PageKite protocol handler takes care of replying */
    }
    else if (0 < PK_HOOK(PK_HOOK_FE_REJECT, 0, ics, NULL)) {
      /* no-op: Hook implemented a custom reply. */
    }
    else if (ics->proto == PKR_PROTO_TLS) {
      pkc_write(&(ics->pkb->conn), PK_REJECT_TLS_DATA, PK_REJECT_TLS_LEN);
    }
    else if (ics->proto == PKR_PROTO_LEGACY_SSL) {
      pkc_write(&(ics->pkb->conn), PK_REJECT_SSL_DATA, PK_REJECT_SSL_LEN);
    }
    else if (ics->proto == PKR_PROTO_HTTP_CONNECT) {
      pkc_write(&(ics->pkb->conn),
                PK_REJECT_HTTP_CONNECT, strlen(PK_REJECT_HTTP_CONNECT));
    }
    else {
      /* Send the HTTP error message by default */
      char hdrs[PK_RESPONSE_MAXSIZE];
      char rej[PK_RESPONSE_MAXSIZE];
      int bytes;

      hdrs[0] = '\0';
      if (ics->parse_state == PARSE_UNDECIDED) {
        sprintf(hdrs, "%s Sorry!\r\n", PK_FRONTEND_OVERLOADED);
      }

      bytes = pk_format_http_rejection(rej, hdrs,
        ics->pkb->conn.sockfd,
        ics->pkm->fancy_pagekite_net_rejection_url,
        "http", ics->hostname);
      pkc_write(&(ics->pkb->conn), rej, bytes);
    }

    ics->pkb->conn.status &= ~CONN_STATUS_CHANGING; /* Change failed */
    pkc_reset_conn(&(ics->pkb->conn), 0);
    pkm_free_be_conn(ics->pkb);
  }
  ics->parse_state = PARSE_FAILED; /* A horrible hack */

  free_if_outside(ics->hostname, ((char*) ics->peeked), ics->peeked_bytes);
  free(ics);
}

#define _PKR_CLEN (10 + PK_DOMAIN_LENGTH + PK_PROTOCOL_LENGTH + PK_SALT_LENGTH*3)
static int _pkr_check_kite_sig(struct pk_manager* pkm,
                               char* raw_request,
                               struct pk_kite_request* kite_r)
{
  /* First, just parse the request */
  char* sig;
  if (NULL == pk_parse_kite_request(kite_r, &sig, raw_request)) {
    return pk_error;
  }

  /* FIXME: Are the requested protocols/ports available? */

  /* Is the request signed? Find salt. */
  unsigned int salt = 0;
  if (sig && *sig) {
    sscanf(sig, "%8x", &salt);
  }
  else {
    return (pk_error = ERR_PARSE_UNSIGNED);
  }

  /* Verify this is a challenge we issued within the last 20 minutes */
  char buf[_PKR_CLEN];
  char* secret = PKRM(pkm)->global_secret;
  char* challenge = raw_request + 11;  /* strlen("X-PageKite:") == 1 */
  while (isspace(*challenge)) challenge++;
  pk_prepare_kite_challenge(buf, kite_r, secret, time(0));
  if (strncmp(buf, challenge, strlen(buf)) != 0) {
    pk_log(PK_LOG_TUNNEL_DATA|PK_LOG_ERROR, "E: %s", buf);
    pk_prepare_kite_challenge(buf, kite_r, secret, time(0) - 600);
    if (strncmp(buf, challenge, strlen(buf)) != 0) {
      pk_log(PK_LOG_TUNNEL_DATA|PK_LOG_ERROR, "E: %s", buf);
      pk_log(PK_LOG_TUNNEL_DATA|PK_LOG_ERROR, "I: %s", challenge);
      pk_log(PK_LOG_TUNNEL_DATA|PK_LOG_ERROR, "Bad FSALT");
      return (pk_error = ERR_PARSE_BAD_FSALT);
    }
  }

  /* FIXME: Check if the request matches a known kite. */

  /* The challenge looks OK, let plugins have a say (for remote auth). */
  int hook_rv = pke_post_blocking_event(&(pkm->events),
                                        PK_EV_FE_AUTH, 0, challenge, 0, NULL);
  if (hook_rv == PK_EV_RESPOND_ACCEPT) return 0;
  if (hook_rv & PK_EV_RESPOND_FALSE) return (pk_error = ERR_PARSE_BAD_SIG);

  /* Assuming a kite was found, check if the secret matches. */
  int bytes = pk_sign_kite_request(buf, kite_r, salt);
  if ((bytes == strlen(raw_request)) &&
      (0 == strncasecmp(buf, raw_request, _PKR_CLEN))) return 0;

  pk_log(PK_LOG_TUNNEL_DATA|PK_LOG_ERROR, "I: %s", raw_request);
  pk_log(PK_LOG_TUNNEL_DATA|PK_LOG_ERROR, "E: %s", buf);

  return (pk_error = ERR_PARSE_BAD_SIG);
}

typedef enum {
  kite_unknown = 0,
  kite_unsigned,
  kite_signed,
  kite_invalid,
  kite_duplicate
} _pkr_rstat;

typedef struct {
  _pkr_rstat             status;
  char*                  raw;
  struct pk_kite_request kite_r;
  struct pk_pagekite     kite;
} _pkr_request;

typedef struct {
  char client_version[PK_HANDSHAKE_VERSION_MAX+1];
  char client_session[PK_HANDSHAKE_SESSIONID_MAX+1];
  _pkr_request requests[PK_HANDSHAKE_REQUEST_MAX];
  int request_count;
  int request_okay;
  int request_problems;
} _pkr_parsed_pagekite_request;

static int _pkr_parse_pagekite_request(struct pk_manager* pkm,
                                        char* buffer, size_t bytes,
                                        _pkr_parsed_pagekite_request* ppr)
{
  strncpyz(ppr->client_version, "(unknown)", PK_HANDSHAKE_VERSION_MAX);
  strcpy(ppr->client_session, "");
  ppr->request_count = 0;
  ppr->request_okay = 0;
  ppr->request_problems = 0;

  pk_error = 0;
  int request_bytes = 0;
  for (char* p = buffer; p < buffer + bytes; ) {
    int skip = zero_first_crlf(bytes - (p - buffer), p);
    request_bytes += skip;

    char* raw_request = p;
    pk_log(PK_LOG_TUNNEL_DATA, "R: %s", p);

    if (p[0] == '\0') {
      return request_bytes;
    }
                     /* 1234567890 = 10 bytes */
    else if (strncasecmp(p, "X-PageKite", 10) == 0) {
      skip -= 10;
      p += 10;

      if ((p[0] == ':') && (p[1] == ' ')) {
        if (ppr->request_count < PK_HANDSHAKE_REQUEST_MAX) {
          _pkr_request* request = &(ppr->requests[ppr->request_count++]);
          request->raw = raw_request;

          /* Note: checking the kite signature has side effects and may
           *       result in callbacks & network requests. One of the side
           *       effects is to populate the request->kite structure.
           */
          request->kite_r.kite = &(request->kite);
          if (0 == _pkr_check_kite_sig(pkm, request->raw, &(request->kite_r))) {
            request->status = kite_signed;
            pk_log(PK_LOG_TUNNEL_DATA|PK_LOG_ERROR, "GOOD: %s", p);
            ppr->request_okay++;
          }
          else {
            if (pk_error == ERR_PARSE_NO_FSALT) {
              request->status = kite_unsigned;
              pk_log(PK_LOG_TUNNEL_DATA|PK_LOG_DEBUG, "Unsigned request");
            }
            else {
              request->status = kite_invalid;
              pk_log(PK_LOG_TUNNEL_DATA|PK_LOG_ERROR, "Request invalid (%x)", pk_error);
            }
            ppr->request_problems++;
          }
        }
        else {
          pk_log(PK_LOG_TUNNEL_DATA|PK_LOG_ERROR, "Too many requests!");
          ppr->request_problems++;
        }
      }
                                 /* 1234567890 = 10 bytes */
      else if (0 == strncasecmp(p, "-Version: ", 10)) {
        strncpyz(ppr->client_version, p + 10, PK_HANDSHAKE_VERSION_MAX);
      }
                                 /* 1234567890 = 10 bytes */
      else if (0 == strncasecmp(p, "-Replace: ", 10)) {
        strncpy(ppr->client_session, p + 10, PK_HANDSHAKE_SESSIONID_MAX);
        ppr->client_session[PK_HANDSHAKE_SESSIONID_MAX] = '\0';
      }
      /* FIXME: Consider the features requested? */
    }

    else if (!IS_PAGEKITE_REQUEST(p) && *p) {
      pk_log(PK_LOG_TUNNEL_DATA, "Bogus data in request: %s", p);
    }

    p += skip;
    pk_error = 0;
  }
  return request_bytes;
}

void pkr_tunnel_cleanup_cb(int unused, struct pk_tunnel* tunnel)
{
  /* FIXME */
  return;
}

static void _pkr_describe_connection(struct pk_chunk* chunk,
                                     struct incoming_conn_state* ics) {
  pk_chunk_reset_values(chunk);
  chunk->sid = ics->pkb->sid;
  chunk->request_host = ics->hostname;
  chunk->request_port = ics->port;
  chunk->request_proto = (char*) proto_protonames[ics->proto];
  //chunk->remote_ip = ics->port;
  //chunk->remote_port = ics->port;
  //chunk->remote_tls = ics->port;
}

static void _pkr_free_description(struct pk_chunk* chunk) {
}

static void _pkr_do_pagekite_handshake(struct pk_blocker* blocker,
                                       struct incoming_conn_state* ics)
{
  struct pk_relay_manager* pkrm = PKRM(ics->pkm);
  struct pk_tunnel* tunnel = NULL;
  struct pk_conn* conn = &(ics->pkb->conn);
  char rbuf[PK_RESPONSE_MAXSIZE];

  /* we have an entire PageKite request in the buffer, parse it! */
  _pkr_parsed_pagekite_request ppr;
  ics->peeked_bytes = _pkr_parse_pagekite_request(ics->pkm, ics->peeked,
                                                  ics->peeked_bytes, &ppr);
  _pkr_consume_peeked(ics);

  pk_log(PK_LOG_TUNNEL_DATA|PK_LOG_ERROR,
         "Client %s requested %d kites: ok=%d, problems=%d, session=%s",
         ppr.client_version,
         ppr.request_count,
         ppr.request_okay,
         ppr.request_problems,
         ppr.client_session);


  if (ppr.request_okay) {
    /* This connection should become a tunnel! Make it so. */

    pkm_reconfig_blocking_start(ics->pkm);
    tunnel = pkm_add_frontend_ai(ics->pkm, NULL, NULL, 0, CONN_STATUS_CHANGING);
    int failed = (tunnel == NULL);
    if (!failed) {
      tunnel->remote_is_be = 1;
      tunnel->remote.be.cleanup_callback_func = (pagekite_callback_t*) pkr_tunnel_cleanup_cb;
      strncpyz(tunnel->remote.be.be_version, ppr.client_version, PK_HANDSHAKE_VERSION_MAX);
      strncpyz(tunnel->session_id, ppr.client_session, PK_HANDSHAKE_SESSIONID_MAX);

      /* Copy the connection data wholesale to our tunnel */
      int status = tunnel->conn.status;
      tunnel->conn = ics->pkb->conn;
      tunnel->conn.status = status;
      conn = &(tunnel->conn);

      /* Prevent ICS cleanup from killing the connection */
      ics->pkb->conn.sockfd = -1;
#ifdef HAVE_OPENSSL
      ics->pkb->conn.ssl = NULL;
#endif
    }
    pkm_reconfig_stop(ics->pkm);

    for (int i = 0; (!failed) && (i < ppr.request_count); i++) {
      if (ppr.requests[i].status == kite_signed) {
        struct pk_pagekite* kite = &(ppr.requests[i].kite);
        failed = (NULL == _pkr_add_kite_info(ics->pkm,
                                             kite->public_domain,
                                             _pkr_parse_proto(kite->protocol),
                                             kite->public_port,
                                             tunnel));
        /* FIXME: Detect duplicates? */
        /* FIXME: Force-disconnect obsolete sessions? */
      }
    }

    if (failed) {
      /* FIXME: Is this a helpful enough error for the backend? */
      pkc_write(conn, rbuf, sprintf(rbuf,
        "%s Sorry!\r\n\r\n", PK_FRONTEND_OVERLOADED));
      _pkr_close(ics, 0, "PageKite tunnel allocation failed");

      /* Mark the tunnel as broken, to trigger cleanup. */
      if (tunnel) {
        tunnel->conn.status |= CONN_STATUS_BROKEN;
        pkm_start_tunnel_io(ics->pkm, tunnel);
        tunnel->conn.status &= ~CONN_STATUS_CHANGING;
      }

      return;
    }
  }

  /* Report back to the client/backend how things went. */

  pkc_write(conn, rbuf, sprintf(rbuf,
    ("%s"
     "Transfer-Encoding: chunked\r\n"
     "X-PageKite-Protos: http,https,raw\r\n"),
    PK_ACCEPT_HTTP));

  /* FIXME: Make the list of protocols dynamic */
  /* FIXME: Report X-PageKite-Ports and X-PageKite-IPv6-Ports */
  /* FIXME: Implement the UUID thing? What's it for? */
  /* FIXME: We have no features, boo! */

  for (int i = 0; i < ppr.request_count; i++) {
    char* request_data = ppr.requests[i].raw + strlen("X-PageKite: ");
    if (ppr.requests[i].status == kite_signed) {
      pkc_write(conn, rbuf, sprintf(rbuf, "X-PageKite-OK: %s\r\n", request_data));
      /* FIXME: Report SSL-OK ? */
    }
    else if ((!ppr.request_okay) && (ppr.requests[i].status == kite_unsigned)) {
      /* No OK requests, signature needed! */
      char np[_PKR_CLEN];
      pk_prepare_kite_challenge(np, &(ppr.requests[i].kite_r),
                                pkrm->global_secret, time(0));
      pkc_write(conn, rbuf, sprintf(rbuf, "X-PageKite-SignThis: %s\r\n", np));
    }
    else {
      /* If some of the requests were OK, we don't prompt the other end
       * to sign (again), we just send an invalid. */
      pkc_write(conn, rbuf, sprintf(rbuf,
        ("X-PageKite-Invalid: %s\r\n"
         "X-PageKite-Invalid-Why: %s;%s\r\n"),
        request_data, request_data, "wtf"));  /* FIXME: why oh why? */
    }
  }
  pkc_write(conn, "\r\n", 2);

  if (tunnel) {
    pkm_start_tunnel_io(ics->pkm, tunnel);
    tunnel->conn.status &= ~CONN_STATUS_CHANGING;
  }
  else _pkr_close(ics, 0, "PageKite request");
}

static void _pkr_connect_tunnel(struct incoming_conn_state* ics) {
  assert(ics->tunnel != NULL);

  ics->pkb->conn.status |= CONN_STATUS_CHANGING;
  if (0 > pkm_configure_sockfd(ics->pkb->conn.sockfd)) {
    _pkr_close(ics, 0, "Failed to configure sockfd");
    return;
  }

  /* Tell the remote end what was requested and who requested it. */
  struct pk_chunk chunk;
  _pkr_describe_connection(&chunk, ics);

  ics->pkb->tunnel = ics->tunnel;
  pkm_write_chunk(ics->tunnel, &chunk);
  pkm_start_be_conn_io(ics->pkm, ics->pkb);

  ics->pkb->conn.status &= ~CONN_STATUS_CHANGING;
  ics->pkb = NULL;

  _pkr_free_description(&chunk);
  _pkr_close(ics, 0, NULL);
}

static int _pkr_process_readable(struct incoming_conn_state* ics)
{
  char* peeked = ics->peeked;
  int bytes;
  int result = PARSE_UNDECIDED;

  PK_TRACE_FUNCTION;

  /* We have data! Peek at it to find out what it is... */
  bytes = PKS_peek(ics->pkb->conn.sockfd, peeked, PEEK_BYTES);
  if (bytes == 0) {
    /* Connection closed already */
    result = PARSE_FAILED;
    ics->peeked_bytes = 0;
  }
  else if (bytes < 0) {
    pk_log(PK_LOG_MANAGER_DEBUG,
           "_pkr_process_readable(%d): peek failed %d=%s!",
           ics->pkb->conn.sockfd, errno, strerror(errno));
    if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK) {
      result = PARSE_WANT_MORE_DATA;
    }
    else {
      result = PARSE_FAILED;
    }
    ics->peeked_bytes = 0;
  }
  else {
    peeked[bytes] = '\0';
    ics->peeked_bytes = bytes;
  }

  if (!result) result = _pkr_parse_ssltls(peeked, bytes, ics);
  if (!result) result = _pkr_parse_http(peeked, bytes, ics);

  /* We always run this, in case the hook wants to modify or suppress
   * the above results. */
  int hook_rv = PK_HOOK(PK_HOOK_FE_PARSE, bytes, peeked, ics);
  if (hook_rv != -1) result = (hook_rv / 10);

  /* No result yet? Do we have enough data? */
  if (!result) {
    if (bytes > 7) {
      /* FIXME: Magic number. Is it even a good one? */
      result = PARSE_FAILED;
    }
    else {
      result = PARSE_WANT_MORE_DATA;
    }
  }

  ics->parse_state = result;

  if ((result == PARSE_WANT_MORE_DATA) || (result == PARSE_MATCH_SLOW)) {
    /* Parsers found something or we still need more data. Punt to another
     * thread for further processing. Punting when we need more data is done
     * to avoid spinning the CPU too badly, which is what would otherwise
     * happen with libev+peeking. */
    pkb_add_job(&(ics->pkm->blocking_jobs), PK_RELAY_INCOMING, result, ics);
  }
  else if (result == PARSE_MATCH_FAST) {
    /* Parsers found something. Process now. */
    pkr_relay_incoming(NULL, result, ics);
  }
  else {
    /* result == PARSE_FAILED */
    _pkr_close(ics, 0, "Parse failed or no tunnel found");
  }

  PK_CHECK_MEMORY_CANARIES;

  return result;
}

/* This function is invoked by pkblocker.c for the PK_RELAY_INCOMING job,
 * or directly by _pkr_process_readable() for "fast" processing.
 */
void pkr_relay_incoming(struct pk_blocker* blocker, int result, void* void_data) {
  struct incoming_conn_state* ics = (struct incoming_conn_state*) void_data;

  PK_TRACE_FUNCTION;

  if (ics->parse_state == PARSE_FAILED)
  {
    pk_log(PK_LOG_TUNNEL_DATA|PK_LOG_ERROR,
           "pkr_relay_incoming() invoked for dead ics");
    return; /* Should never happen */
  }
  else if (result == PARSE_WANT_MORE_DATA)
  {
    /* This happens when we've punted a connection to a blocker thread
     * instead of handling it on the main event loop.
     */
    if (pk_time() - ics->created > 5) {
      /* FIXME: Magic number
       * We don't wait forever for more data; that would make resource
       * exhaustion DOS attacks against the server trivially easy. */
      _pkr_close(ics, 0, "Timed out");
    }
    else {
      /* Slow down a tiny bit, unless there are jobs in the queue waiting
       * for us to finish. */
      if (ics->pkm->blocking_jobs.count < 1) sleep_ms(100);

      /* Note: This will not exhaust the stack because PARSE_WANT_MORE_DATA
       * always goes through the job queue and runs on a blocking thread.
       */
      _pkr_process_readable(ics);
    }
  }
  else
  {
    /* If we get this far, then the request has (probably) been parsed and
     * we have in ics details about what needs to be done. So we do it! :)
     */
    if (result == PARSE_MATCH_FAST || result == PARSE_MATCH_SLOW)
    {
      if (ics->tunnel) {
        if (ics->proto == PKR_PROTO_HTTP_CONNECT) {
          pkc_write(&(ics->pkb->conn),
                    PK_ACCEPT_HTTP_CONNECT, strlen(PK_ACCEPT_HTTP_CONNECT));
          _pkr_consume_peeked(ics);
        }
        _pkr_connect_tunnel(ics);
      }
      else if (ics->proto == PKR_PROTO_PAGEKITE) {
        _pkr_do_pagekite_handshake(blocker, ics);
      }
      else if (ics->proto == PKR_PROTO_PAGEKITE_PING) {
        _pkr_close(ics, 0, NULL);
      }
      else {
        _pkr_close(ics, 0, "No tunnel found");
      }
    }
    else {
      _pkr_close(ics, 0, "No tunnel found");
    }
  }
}

void pkr_new_conn_readable_cb(EV_P_ ev_io* w, int revents)
{
  struct incoming_conn_state* ics = (struct incoming_conn_state*) w->data;

  PK_TRACE_FUNCTION;

  ev_io_stop(EV_A_ w);
  _pkr_process_readable(ics);

  /* -Wall dislikes unused arguments */
  (void) loop;
  (void) revents;
}

void pkr_conn_accepted_cb(int sockfd, int ralen, void* void_ra, void* void_pkm)
{
  static int sid_counter = 0;  /* Note: Not thread-safe, but still OK. */
  struct incoming_conn_state* ics;
  struct pk_manager* pkm = (struct pk_manager*) void_pkm;
  struct pk_backend_conn* pkb;
  socklen_t slen;
  char rbuf[128];
  char lbuf[128];

  PK_TRACE_FUNCTION;

  ics = malloc(sizeof(struct incoming_conn_state));
  ics->pkm = pkm;
  ics->pkb = NULL;
  ics->tunnel = NULL;
  ics->hostname = NULL;
  ics->proto = PKR_PROTO_NONE;
  ics->parse_state = PARSE_UNDECIDED;
  ics->created = pk_time();

  slen = sizeof(ics->local_addr);
  getsockname(sockfd, (struct sockaddr*) &(ics->local_addr), &slen);
#ifdef HAVE_IPV6
  ics->port = ntohs(
    (((struct sockaddr*) &(ics->local_addr))->sa_family == AF_INET)
      ? ((struct sockaddr_in*) &(ics->local_addr))->sin_port
      : ics->local_addr.sin6_port);
#else
  ics->port = ntohs(ics->local_addr.sin_port);
#endif
  memcpy(&(ics->remote_addr), void_ra, ralen);

  pk_log(PK_LOG_TUNNEL_DATA,
         "Accepted; remote=%s; local=%s; port=%d; fd=%d",
         in_addr_to_str((struct sockaddr*) &(ics->remote_addr), rbuf, 128),
         in_addr_to_str((struct sockaddr*) &(ics->local_addr), lbuf, 128),
         ics->port, sockfd);

  /* Generate a stream ID; it is OK for these to repeat, but we need to
   * be sure that they are unique at any given time. Using the sockfd
   * should accomplish that easily while also keeping the SIDs short as
   * long as load is moderate. This all falls apart if we start working
   * with over a million sockets or the OS stops assigning the numbers
   * linearly... */
  assert(PK_MAX_SID_SIZE >= 8);
  if (sockfd > 0xffffff) {
    pk_log(PK_LOG_TUNNEL_DATA|PK_LOG_ERROR,
           "SIDs would overlap (sockfd %d > %d)", sockfd, 0xffffff);
    _pkr_close(ics, PK_LOG_ERROR, "BE alloc failed");
    /* We need to close the socket manually, since ics->pkb is unset */
    PKS_close(sockfd);
    return;
  }
  sprintf(lbuf, "%x%.2X", sockfd, (sid_counter++ & 0xff));

  /* Allocate a connection for this request or die... */
  if (NULL == (pkb = pkm_alloc_be_conn(pkm, NULL, lbuf))) {
    _pkr_close(ics, PK_LOG_ERROR, "BE alloc failed");
    /* We need to close the socket manually, since ics->pkb is unset */
    PKS_close(sockfd);
    return;
  }

  pkb->kite = NULL;
  pkb->conn.sockfd = sockfd;
  ics->pkb = pkb;

  set_non_blocking(sockfd);

  ev_io_init(&(pkb->conn.watch_r),
             pkr_new_conn_readable_cb,
             PKS_EV_FD(sockfd),
             EV_READ);
  pkb->conn.watch_r.data = (void *) ics;
  ev_io_start(pkm->loop, &(pkb->conn.watch_r));
}


/****************************************************************************
 * These are the public library methods; we define them here instead of in
 * pagekite.c to simplify building the library without AGPLv3 code.
 */

int pagekite_add_relay_listener(pagekite_mgr pkm, int port, int flags)
{
  if (pkm == NULL) return -1;
  struct pk_manager* m = (struct pk_manager*) pkm;

  if (flags & PK_WITH_DEFAULTS) flags |= (PK_WITH_IPV6 | PK_WITH_IPV4);

#ifdef HAVE_IPV6
  if (flags & PK_WITH_IPV6) {
    return pkm_add_listener(m, "::", port,
                            (pagekite_callback2_t*) &pkr_conn_accepted_cb,
                            (void*) m);
  }
#endif
  if (flags & PK_WITH_IPV4) {
    return pkm_add_listener(m, "0.0.0.0", port,
                            (pagekite_callback2_t*) &pkr_conn_accepted_cb,
                            (void*) m);
  }
  return (pk_error = ERR_NO_IPVX);
}


/****************************************************************************
 * Unit tests...
 */

int pkrelay_test(void)
{
#if PK_TESTS
  unsigned char* test_http = "GET / HTTP/1.0\r\nHost: foo.bar\r\n\r\n";
  int test_tls_sni_len = 332;
  unsigned char test_tls_sni[332] = {
    0x16, 0x03, 0x01, 0x01, 0x47, 0x01, 0x00, 0x01, 0x43, 0x03, 0x03, 0x62, 0xa4, 0xde, 0x83, 0x9c,
    0x34, 0xfb, 0x03, 0x4d, 0xe0, 0x34, 0x17, 0x00, 0xc9, 0x3c, 0xd3, 0x92, 0xfc, 0xf4, 0xee, 0xaf,
    0xbb, 0xa0, 0x62, 0x21, 0x9e, 0x79, 0x8f, 0x1c, 0x00, 0x3d, 0x5a, 0x00, 0x00, 0xb4, 0xc0, 0x30,
    0xc0, 0x2c, 0xc0, 0x28, 0xc0, 0x24, 0xc0, 0x14, 0xc0, 0x0a, 0x00, 0xa5, 0x00, 0xa3, 0x00, 0xa1,
    0x00, 0x9f, 0x00, 0x6b, 0x00, 0x6a, 0x00, 0x69, 0x00, 0x68, 0x00, 0x39, 0x00, 0x38, 0x00, 0x37,
    0x00, 0x36, 0x00, 0x88, 0x00, 0x87, 0x00, 0x86, 0x00, 0x85, 0xc0, 0x32, 0xc0, 0x2e, 0xc0, 0x2a,
    0xc0, 0x26, 0xc0, 0x0f, 0xc0, 0x05, 0x00, 0x9d, 0x00, 0x3d, 0x00, 0x35, 0x00, 0x84, 0xc0, 0x2f,
    0xc0, 0x2b, 0xc0, 0x27, 0xc0, 0x23, 0xc0, 0x13, 0xc0, 0x09, 0x00, 0xa4, 0x00, 0xa2, 0x00, 0xa0,
    0x00, 0x9e, 0x00, 0x67, 0x00, 0x40, 0x00, 0x3f, 0x00, 0x3e, 0x00, 0x33, 0x00, 0x32, 0x00, 0x31,
    0x00, 0x30, 0x00, 0x9a, 0x00, 0x99, 0x00, 0x98, 0x00, 0x97, 0x00, 0x45, 0x00, 0x44, 0x00, 0x43,
    0x00, 0x42, 0xc0, 0x31, 0xc0, 0x2d, 0xc0, 0x29, 0xc0, 0x25, 0xc0, 0x0e, 0xc0, 0x04, 0x00, 0x9c,
    0x00, 0x3c, 0x00, 0x2f, 0x00, 0x96, 0x00, 0x41, 0xc0, 0x11, 0xc0, 0x07, 0xc0, 0x0c, 0xc0, 0x02,
    0x00, 0x05, 0x00, 0x04, 0xc0, 0x12, 0xc0, 0x08, 0x00, 0x16, 0x00, 0x13, 0x00, 0x10, 0x00, 0x0d,
    0xc0, 0x0d, 0xc0, 0x03, 0x00, 0x0a, 0x00, 0x15, 0x00, 0x12, 0x00, 0x0f, 0x00, 0x0c, 0x00, 0x09,
    0x00, 0xff, 0x01, 0x00, 0x00, 0x66, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x0b, 0x00, 0x00, 0x08, 0x73,
    0x6e, 0x69, 0x2e, 0x74, 0x65, 0x73, 0x74, 0x00, 0x0b, 0x00, 0x04, 0x03, 0x00, 0x01, 0x02, 0x00,
    0x0a, 0x00, 0x1c, 0x00, 0x1a, 0x00, 0x17, 0x00, 0x19, 0x00, 0x1c, 0x00, 0x1b, 0x00, 0x18, 0x00,
    0x1a, 0x00, 0x16, 0x00, 0x0e, 0x00, 0x0d, 0x00, 0x0b, 0x00, 0x0c, 0x00, 0x09, 0x00, 0x0a, 0x00,
    0x23, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x20, 0x00, 0x1e, 0x06, 0x01, 0x06, 0x02, 0x06, 0x03, 0x05,
    0x01, 0x05, 0x02, 0x05, 0x03, 0x04, 0x01, 0x04, 0x02, 0x04, 0x03, 0x03, 0x01, 0x03, 0x02, 0x03,
    0x03, 0x02, 0x01, 0x02, 0x02, 0x02, 0x03, 0x00, 0x0f, 0x00, 0x01, 0x01};
  unsigned char* sni_domain;

  /* Basic TLS SNI parsing tests... */
  assert(NULL == _pkr_get_first_tls_sni(test_http, strlen(test_http)));
  sni_domain = _pkr_get_first_tls_sni(test_tls_sni, test_tls_sni_len);
  assert(sni_domain != NULL);
  assert(0 == strcmp("sni.test", sni_domain));
  /* Corrupt the data to make it hostile */
  test_tls_sni[0xee] = 0xff;
  assert(NULL ==_pkr_get_first_tls_sni(test_tls_sni, test_tls_sni_len));
  /* FIXME: Test more corruption. */
  fprintf(stderr, "TLS SNI parsing tests passed\n");

  /* Cases to test:
   *   - Incoming authorized tunnel request
   *   - Incoming unauthorized tunnel request
   *   - Incoming HTTP requests
   *   - Incoming HTTP Proxy requests
   *   - Incoming end-to-end HTTPS requests
   *   - Incoming locally terminated SSL connections
   *   - Unrecognized incoming data
   */

  fprintf(stderr, "FIXME: Test more stuff in pkrelay.c\n");
#endif
  return 1;
}


/* ***************************************************************************
This is how we generated the SNI test data:

(
  sleep 1;
  openssl s_client -connect localhost:9443 -quiet -servername sni.test
) & nc -l 9443 |hexdump -C
<CTRL-C>

Results:

00000000  16 03 01 01 47 01 00 01  43 03 03 62 a4 de 83 9c  |....G...C..b....|
00000010  34 fb 03 4d e0 34 17 00  c9 3c d3 92 fc f4 ee af  |4..M.4...<......|
00000020  bb a0 62 21 9e 79 8f 1c  00 3d 5a 00 00 b4 c0 30  |..b!.y...=Z....0|
00000030  c0 2c c0 28 c0 24 c0 14  c0 0a 00 a5 00 a3 00 a1  |.,.(.$..........|
00000040  00 9f 00 6b 00 6a 00 69  00 68 00 39 00 38 00 37  |...k.j.i.h.9.8.7|
00000050  00 36 00 88 00 87 00 86  00 85 c0 32 c0 2e c0 2a  |.6.........2...*|
00000060  c0 26 c0 0f c0 05 00 9d  00 3d 00 35 00 84 c0 2f  |.&.......=.5.../|
00000070  c0 2b c0 27 c0 23 c0 13  c0 09 00 a4 00 a2 00 a0  |.+.'.#..........|
00000080  00 9e 00 67 00 40 00 3f  00 3e 00 33 00 32 00 31  |...g.@.?.>.3.2.1|
00000090  00 30 00 9a 00 99 00 98  00 97 00 45 00 44 00 43  |.0.........E.D.C|
000000a0  00 42 c0 31 c0 2d c0 29  c0 25 c0 0e c0 04 00 9c  |.B.1.-.).%......|
000000b0  00 3c 00 2f 00 96 00 41  c0 11 c0 07 c0 0c c0 02  |.<./...A........|
000000c0  00 05 00 04 c0 12 c0 08  00 16 00 13 00 10 00 0d  |................|
000000d0  c0 0d c0 03 00 0a 00 15  00 12 00 0f 00 0c 00 09  |................|
000000e0  00 ff 01 00 00 66 00 00  00 0d 00 0b 00 00 08 73  |.....f.........s|
000000f0  6e 69 2e 74 65 73 74 00  0b 00 04 03 00 01 02 00  |ni.test.........|
00000100  0a 00 1c 00 1a 00 17 00  19 00 1c 00 1b 00 18 00  |................|
00000110  1a 00 16 00 0e 00 0d 00  0b 00 0c 00 09 00 0a 00  |................|
00000120  23 00 00 00 0d 00 20 00  1e 06 01 06 02 06 03 05  |#..... .........|
00000130  01 05 02 05 03 04 01 04  02 04 03 03 01 03 02 03  |................|
00000140  03 02 01 02 02 02 03 00  0f 00 01 01              |............|

*************************************************************************** */
