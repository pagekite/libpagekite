/******************************************************************************
pkrelay.c - Logic specific to front-end relays

*******************************************************************************

This file is Copyright 2011-2015, The Beanstalks Project ehf.

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
connection has been identified, the same event loop as handles connectors
will work for relays.

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

#include "pagekite.h"

#define _GNU_SOURCE
#include <ctype.h>
#include <string.h>

#include "common.h"
#include "utils.h"
#include "pkstate.h"
#include "pkerror.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pkrelay.h"
#include "pklogging.h"

#define PROTO_WANT_MORE_DATA 2
#define PROTO_MATCHED 1
#define PROTO_UNDECIDED 0
#define PROTO_FAILED -1

#define PEEK_BYTES 512
#define SSL_CLIENTHELLO 0x80
#define TLS_CLIENTHELLO 0x16

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

static int _pkr_handle_ssltls(EV_P_ ev_io* w,
                              struct pk_backend_conn* pkb,
                              unsigned char* peeked,
                              int bytes)
{
  /* Is this TLS? */
  if (peeked[0] == TLS_CLIENTHELLO) {
    char* tls_sni = _pkr_get_first_tls_sni(peeked, bytes);
    if (tls_sni == TLS_PARSE_FAILED) return PROTO_FAILED;

    /* Is it a connection to a remote tunneled backend?
     * Is it a connection to one of our own certs?
     * None of the above: eat the data, return an error
     */
    fprintf(stderr, "Is TLS: %s\n", tls_sni);
    return PROTO_FAILED;
  }
  else if (peeked[0] == SSL_CLIENTHELLO) {
    /* FIXME: Client wants legacy SSL - do we have a certificate? */
    fprintf(stderr, "Is legacy SSL\n");
    return PROTO_FAILED;
  }

  /* -Wall dislikes unused arguments */
  (void) loop;
  (void) bytes;
  return PROTO_UNDECIDED;
}


static int _pkr_handle_http(EV_P_ ev_io* w,
                            struct pk_backend_conn* pkb,
                            char* peeked,
                            int bytes)
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
  if (sp == NULL) return PROTO_UNDECIDED;

  char* nl = strchr(peeked, '\n');
  if ((nl == NULL) || (nl < sp)) return PROTO_UNDECIDED;

  char* http = strcasestr(peeked, " HTTP/");
  if (http && (sp < http) && (nl > http) && isalpha(peeked[0])) {
    /* - Is it a connection to a remote tunneled backend?
     * - Is it a connection to an internal LUA HTTPd?
     * - Is it a PageKite tunnel request?
     * - None of the above: eat the data, return an error
     */
    char* host = strcasestr(peeked, "\nHost: ");
    if (host) {
      host += 7;
      char* eol = strchr(host, '\n');
      if (eol) *eol-- = '\0';
      if (eol && (*eol == '\r')) *eol = '\0';
      eol = strchr(host, ':');
      if (eol) *eol = '\0';
    }
    fprintf(stderr, "Is HTTP %s\n", host ? host : "(unknown)");
    return PROTO_FAILED;
  }

  /* -Wall dislikes unused arguments */
  (void) loop;
  (void) bytes;
  return PROTO_UNDECIDED;
}

static void pkr_new_conn_readable_cb(EV_P_ ev_io* w, int revents)
{
  struct pk_backend_conn* pkb = (struct pk_backend_conn*) w->data;
  char peeked[PEEK_BYTES+1];
  int bytes;
  
  PK_TRACE_FUNCTION;

  /* We have data! Peek at it to find out what it is... */
  bytes = PKS_peek(pkb->conn.sockfd, peeked, PEEK_BYTES);
  peeked[bytes] = '\0';

  int result = (_pkr_handle_ssltls(EV_A_ w,
                                   pkb, (unsigned char*) peeked, bytes) ||
                _pkr_handle_http(EV_A_ w, pkb, peeked, bytes) ||
                PROTO_FAILED);

  /* FIXME: Does this belong to a LUA plugin? Handle more protocols? */

  if (result != PROTO_WANT_MORE_DATA) {
    ev_io_stop(EV_A_ w);
    close(pkb->conn.sockfd);
  }

  PK_CHECK_MEMORY_CANARIES;

  /* -Wall dislikes unused arguments */
  (void) loop;
  (void) revents;
}

static void pkr_conn_accepted_cb(int sockfd, void* void_pkm)
{
  struct pk_manager* pkm = (struct pk_manager*) void_pkm;
  struct pk_backend_conn* pkb;

  /* Allocate a connection for this request or die... */
  if (NULL == (pkb = pkm_alloc_be_conn(pkm, NULL, "!NEW"))) {
    pk_log(PK_LOG_TUNNEL_CONNS|PK_LOG_ERROR,
           "pkr_conn_accepted_cb: BE alloc failed for %d", sockfd);
    close(sockfd);
  }

  pkb->kite = NULL;
  pkb->conn.sockfd = sockfd;

  ev_io_init(&(pkb->conn.watch_r),
             pkr_new_conn_readable_cb,
             PKS_EV_FD(sockfd),
             EV_READ);
  pkb->conn.watch_r.data = (void *) pkb;
  ev_io_start(pkm->loop, &(pkb->conn.watch_r));
}


/****************************************************************************
 * These are the public library methods; we define them here instead of in
 * pagekite.c to simplify building the library without AGPLv3 code.
 */

int pagekite_add_relay_listener(pagekite_mgr pkm, int port)
{
  if (pkm == NULL) return -1;
  struct pk_manager* m = (struct pk_manager*) pkm;
  return pkm_add_listener(m, "0.0.0.0", port,
                          (pagekite_callback_t*) &pkr_conn_accepted_cb,
                          (void*) m);
}

