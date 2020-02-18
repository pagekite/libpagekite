/******************************************************************************
pkrelay.c - Logic specific to front-end relays

*******************************************************************************

This file is Copyright 2011-2020, The Beanstalks Project ehf.

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
#include "pkblocker.h"
#include "pkmanager.h"
#include "pkrelay.h"
#include "pklogging.h"

#define PARSE_FAILED         -2
#define PARSE_WANT_MORE_DATA -1
#define PARSE_UNDECIDED       0
#define PARSE_MATCH_FAST      1
#define PARSE_MATCH_SLOW      2

#define PROTO_UNKNOWN         0
#define PROTO_PAGEKITE        1
#define PROTO_HTTP            2
#define PROTO_HTTP_CONNECT    3
#define PROTO_TLS_WITH_SNI    4
#define PROTO_LEGACY_SSL      5
static const char* known_protos[] = {
  "(unknown)",
  "PageKite",
  "HTTP",
  "HTTP CONNECT",
  "TLS+SNI",
  "SSL/TLS"};

#define PEEK_BYTES 512
#define SSL_CLIENTHELLO 0x80
#define TLS_CLIENTHELLO 0x16

struct incoming_conn_state {
  struct sockaddr_in local_addr;
  struct sockaddr_in remote_addr;
  struct pk_manager* pkm;
  struct pk_backend_conn* pkb;
  struct pk_tunnel* tunnel;
  unsigned char* hostname;
  unsigned char* unparsed_data;
  int parsed_as;
  int parse_state;
  time_t created;
};


static int _find_tunnel(struct incoming_conn_state* ics)
{
  /* FIXME: This is where scalability is important; we need this to
   *        be sub-linear in connection count. The faster the better.
   */
  return 0;
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
    if (tls_sni == TLS_PARSE_FAILED) return PARSE_FAILED;
    ics->hostname = tls_sni;
    ics->parsed_as = tls_sni ? PROTO_TLS_WITH_SNI : PROTO_LEGACY_SSL;
    return _find_tunnel(ics) ? PARSE_MATCH_FAST : PARSE_MATCH_SLOW;
  }
  else if (peeked[0] == SSL_CLIENTHELLO) {
    ics->parsed_as = PROTO_LEGACY_SSL;
    pk_log(PK_LOG_MANAGER_DEBUG, "FIXME: Is legacy SSL. Have cert?");
    return PARSE_MATCH_SLOW;
  }

  /* -Wall dislikes unused arguments */
  (void) bytes;
  return PARSE_UNDECIDED;
}


#define IS_PAGEKITE_REQUEST(p) (strncasecmp(p, PK_HANDSHAKE_CONNECT, \
                                            strlen(PK_HANDSHAKE_CONNECT) \
                                            ) == 0)
static int _pkr_parse_pagekite_request(char* peeked,
                                       int bytes,
                                       char* first_nl,
                                       struct incoming_conn_state* ics)
{
  char *end;
  /* Since we know pagekite requests are small and peekable, we
   * can check if it's complete and process if so, otherwise defer.
   */
  if ((*(first_nl-1) == '\r') ? ((end = strstr(peeked, "\r\n\r\n")) != NULL)
                              : ((end = strstr(peeked, "\n\n")) != NULL))
  {
    pk_log(PK_LOG_MANAGER_DEBUG, "FIXME: Complete PageKite request!");
    /* For now, we punt on actually parsing the incoming data and let
     * that happen in an external implementation. */
    ics->parsed_as = PROTO_PAGEKITE;
    ics->unparsed_data = (char*) malloc(end - peeked + 1);
    memcpy(ics->unparsed_data, peeked, end - peeked);
    ics->unparsed_data[end - peeked] = '\0';
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

    /* - Is it a PageKite tunnel request? */
    if (IS_PAGEKITE_REQUEST(peeked)) {
      return _pkr_parse_pagekite_request(peeked, bytes, nl, ics);
    }

    /* - Is it an HTTP Proxy request? */
    if (strncasecmp(peeked, "CONNECT ", 8) == 0) {
      /* HTTP PROXY requests are one-liners. If it's valid, we start
       * tunneling, if it's not, we close. We complete the handshake
       * on another thread, just in case it blocks. */
      ics->hostname = peeked + 8;
      ics->parsed_as = PROTO_HTTP_CONNECT;
      zero_first_whitespace(bytes - 8, ics->hostname);
      return _find_tunnel(ics) ? PARSE_MATCH_SLOW : PARSE_FAILED;
    }

    /* The other HTTP implementations all prefer the Host: header */
    ics->parsed_as = PROTO_HTTP;
    char* host = strcasestr(peeked, "\nHost: ");
    if (host) {
      host += 7;
      if (zero_first_whitespace(bytes - (host - peeked), host)) {
        ics->hostname = host;
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

    if (_find_tunnel(ics)) return PARSE_MATCH_FAST;

    /* FIXME: Is it a connection to an internal HTTPD of some sort? */

    return PARSE_FAILED;
  }

  /* -Wall dislikes unused arguments */
  (void) bytes;
  return PARSE_UNDECIDED;
}

static void _pkr_close(struct incoming_conn_state* ics,
                       int log_level,
                       const char* reason)
{
  char rbuf[128];
  char lbuf[128];

  /* FIXME: Log more useful things about the connection. */
  pk_log(PK_LOG_TUNNEL_CONNS | log_level,
        "%s; remote=%s; local=%s; hostname=%s; proto=%s; fd=%d",
        reason,
        in_addr_to_str((struct sockaddr*) &(ics->remote_addr), rbuf, 128),
        in_addr_to_str((struct sockaddr*) &(ics->local_addr), lbuf, 128),
        ics->hostname,
        known_protos[ics->parsed_as],
        ics->pkb->conn.sockfd);

  if (ics->pkb) {
    pkc_reset_conn(&(ics->pkb->conn), 0);
    pkm_free_be_conn(ics->pkb);
  }
  ics->parse_state = PARSE_FAILED; /* A horrible hack */

  if (ics->unparsed_data != NULL) free(ics->unparsed_data);
  free(ics);
}

static int _pkr_process_readable(struct incoming_conn_state* ics)
{
  char peeked[PEEK_BYTES+1];
  int bytes;
  int result = PARSE_UNDECIDED;

  PK_TRACE_FUNCTION;

  /* We have data! Peek at it to find out what it is... */
  bytes = PKS_peek(ics->pkb->conn.sockfd, peeked, PEEK_BYTES);
  if (bytes == 0) {
    /* Connection closed already */
    result = PARSE_FAILED;
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
  }
  else {
    peeked[bytes] = '\0';
  }

  if (!result) result = _pkr_parse_ssltls(peeked, bytes, ics);
  if (!result) result = _pkr_parse_http(peeked, bytes, ics);

  /* No result yet? Do we have enough data? */
  if (!result) {
    if (bytes > 4) {
      result = PARSE_FAILED;
    }
    else {
      result = PARSE_WANT_MORE_DATA;
    }
  }

  ics->parse_state = result;
  pk_log(PK_LOG_MANAGER_DEBUG,
         "_pkr_process_readable(%d): result=%d, bytes=%d",
         ics->pkb->conn.sockfd, result, bytes);

  if ((result == PARSE_WANT_MORE_DATA) || (result == PARSE_MATCH_SLOW)) {
    /* Parsers found something or we still need more data.
     * Punt to another thread for further processing. */
    pkb_add_job(&(ics->pkm->blocking_jobs), PK_RELAY_INCOMING, result, ics);
  }
  else if (result == PARSE_MATCH_FAST) {
    /* Parsers found something. Process now. */
    pkr_relay_incoming(result, ics);
  }
  else {
    /* result == PARSE_FAILED */
    _pkr_close(ics, 0, "No tunnel found");
  }

  PK_CHECK_MEMORY_CANARIES;

  return result;
}

void pkr_relay_incoming(int result, void* void_data) {
  struct incoming_conn_state* ics = (struct incoming_conn_state*) void_data;

  PK_TRACE_FUNCTION;

  if (ics->parse_state == PARSE_FAILED) {
    pk_log(PK_LOG_TUNNEL_DATA|PK_LOG_ERROR,
           "pkr_relay_incoming() invoked for dead ics");
    return; /* Should never happen */
  }

  if (result == PARSE_WANT_MORE_DATA) {
    if (pk_time() - ics->created > 5) {
      /* We don't wait forever for more data; that would make resource
       * exhaustion DOS attacks against the server trivially easy. */
      _pkr_close(ics, 0, "Timed out");
    }
    else {
      /* Slow down a tiny bit, unless there are jobs in the queue waiting
       * for us to finish. */
      if (ics->pkm->blocking_jobs.count < 1) sleep_ms(100);
      _pkr_process_readable(ics);
    }
  }
  else {
    /* If we get this far, then the request has been parsed and we have
     * in ics details about what needs to be done. So we do it! :)
     */

    if (ics->parsed_as == PROTO_PAGEKITE) {
      char* response = NULL;
      struct pke_event* ev = pke_post_blocking_event(NULL,
        PK_EV_TUNNEL_REQUEST, 0, ics->unparsed_data, NULL, &response);

      int rlen = 0;
      if (response && *response) {
        rlen = strlen(response);
        pkc_write(&(ics->pkb->conn), response, rlen);
      }

      int result_ok = rlen && ev && (ev->response_code & PK_EV_RESPOND_TRUE);
      if (result_ok) {
        /* FIXME: Upgrade this connection to a tunnel */

        int flying = 0;
        struct pk_kite_request* pkr = pk_parse_pagekite_response(
          response, rlen + 1, NULL, NULL);
        if (pkr != NULL) {
          struct pk_kite_request* p;
          for (p = pkr; p->status != PK_KITE_UNKNOWN; p++) {
            if (p->status == PK_KITE_FLYING) {
              /* FIXME: Record kite->tunnel mapping */
              pk_log(PK_LOG_MANAGER_DEBUG, "Accepted kite: %s://%s:%d",
                                           p->kite->protocol,
                                           p->kite->public_domain,
                                           p->kite->public_port);
              flying++;
            }
            else {
              pk_log(PK_LOG_MANAGER_DEBUG, "Rejected kite: %s://%s:%d (%d)",
                                           p->kite->protocol,
                                           p->kite->public_domain,
                                           p->kite->public_port,
                                           p->status);
            }
          }
          free(pkr);

          /* FIXME: Add to event loop */
        }

        if (!flying) result_ok = 0;
      }

      if (ev) pke_free_event(NULL, ev->event_code);
      if (response) free(response);
      if (!result_ok) _pkr_close(ics, 0, "Rejected");
    }
    else {
      _pkr_close(ics, 0, "FIXME");
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

int pkr_conn_accepted_cb(int sockfd, void* void_pkm)
{
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
  ics->parsed_as = PROTO_UNKNOWN;
  ics->parse_state = PARSE_UNDECIDED;
  ics->created = pk_time();
  ics->unparsed_data = NULL;

  slen = sizeof(ics->local_addr);
  getsockname(sockfd, (struct sockaddr*) &(ics->local_addr), &slen);

  slen = sizeof(ics->remote_addr);
  getpeername(sockfd, (struct sockaddr*) &(ics->remote_addr), &slen);

  pk_log(PK_LOG_TUNNEL_DATA,
         "Accepted; remote=%s; local=%s; fd=%d",
         in_addr_to_str((struct sockaddr*) &(ics->remote_addr), rbuf, 128),
         in_addr_to_str((struct sockaddr*) &(ics->local_addr), lbuf, 128),
         sockfd);

  /* Allocate a connection for this request or die... */
  sprintf(lbuf, "!NEW:%d", ics->remote_addr.sin_port);
  if (NULL == (pkb = pkm_alloc_be_conn(pkm, NULL, lbuf))) {
    _pkr_close(ics, PK_LOG_ERROR, "BE alloc failed");
    PKS_close(sockfd);
    return -1;
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
  return 0;
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
                            (pagekite_callback_t*) &pkr_conn_accepted_cb,
                            (void*) m);
  }
#endif
  if (flags & PK_WITH_IPV4) {
    return pkm_add_listener(m, "0.0.0.0", port,
                            (pagekite_callback_t*) &pkr_conn_accepted_cb,
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
