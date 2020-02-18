/******************************************************************************
pkproto.h - A basic serializer/deserializer for the PageKite tunnel protocol.

This file is Copyright 2011-2020, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify it under
the terms  of the  Apache  License 2.0  as published by the  Apache  Software
Foundation.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the Apache License for more details.

You should have received a copy of the Apache License along with this program.
If not, see: <http://www.apache.org/licenses/>

Note: For alternate license terms, see the file COPYING.md.

******************************************************************************/

#include "pagekite.h"

/* This magic number is a high estimate of how much overhead we expect the
 * PageKite frame and chunk headers to add to each sent packet.
 *
 * 12345Z1234\r\nSID: 123456789\r\n\r\n = 30 bytes, so double that.
 */
#define PROTO_OVERHEAD_PER_KB  64

#define PK_FRONTEND_PING "GET /ping HTTP/1.1\r\nHost: ping.pagekite\r\n\r\n"
#define PK_FRONTEND_PONG "HTTP/1.1 503 Unavailable"
#define PK_FRONTEND_UUID "X-PageKite-UUID:"
#define PK_FRONTEND_OVERLOADED "X-PageKite-Overloaded:"

#define PK_HANDSHAKE_CONNECT "CONNECT PageKite:1 HTTP/1.0\r\n"
#ifdef ANDROID
#define PK_HANDSHAKE_FEATURES ("X-PageKite-Features: Mobile\r\n" \
                               "X-PageKite-Version: " PK_VERSION "\r\n")
#else
#define PK_HANDSHAKE_FEATURES ("X-PageKite-Version: " PK_VERSION "\r\n")
#endif
#define PK_HANDSHAKE_SESSION "X-PageKite-Replace: %s\r\n"
#define PK_HANDSHAKE_KITE "X-PageKite: %s\r\n"
#define PK_HANDSHAKE_END "\r\n"
#define PK_HANDSHAKE_SESSIONID_MAX 256

/* Must be careful here, outsiders can manipulate the contents of the
 * reply message.  Beware the buffer overflows! */
#define PK_REJECT_BACKEND -1
#define PK_REJECT_MAXSIZE 1024
#define PK_REJECT_FMT (\
  "HTTP/1.1 503 Unavailable\r\n" \
  "Content-Type: text/html; charset=utf-8\r\n" \
  "Pragma: no-cache\r\n" \
  "Expires: 0\r\n" \
  "Cache-Control: no-store\r\n" \
  "Connection: close\r\n" \
  "\r\n" \
  "<html>%.450s<h1>Sorry! (%.2s/%.16s)</h1><p>The %.8s" \
  " <a href='http://pagekite.org/'><i>PageKite</i></a> for <b>%.128s</b>" \
  " is unavailable at the moment.</p><p>Please try again later.</p>" \
  "%.64s</html>")
#define PK_REJECT_FANCY_URL "https://pagekite.net/offline/"
#define PK_REJECT_FANCY_PRE (\
  "<frameset cols='*'><frame target='_top' " \
  "src='%.128s?&where=%.2s&v=%.16s&proto=%.8s&domain=%.64s&relay_ip=%.40s'>" \
  "<noframes>")
#define PK_REJECT_FANCY_POST (\
  "</noframes></frameset>")

#define PK_REJECT_TLS_DATA "\x15\x03\0\0\x02\x02\x31"
#define PK_REJECT_TLS_LEN  7

#define PK_EOF_READ  0x1
#define PK_EOF_WRITE 0x2
#define PK_EOF       (PK_EOF_READ | PK_EOF_WRITE)

/* Data structure describing a kite */
#define PK_PROTOCOL_LENGTH   24
#define PK_DOMAIN_LENGTH   1024
#define PK_SECRET_LENGTH    256
struct  pk_pagekite {
  PK_MEMORY_CANARY
  char  protocol[PK_PROTOCOL_LENGTH+1];
  char  public_domain[PK_DOMAIN_LENGTH+1];
  int   public_port;
  char  local_domain[PK_DOMAIN_LENGTH+1];
  int   local_port;
  char  auth_secret[PK_SECRET_LENGTH+1];
};

/* Data structure describing a kite request */
#define PK_KITE_UNKNOWN   0x0000
#define PK_KITE_OK        0x0001
#define PK_KITE_FLYING    0x0001  /* Same as OK */
#define PK_KITE_WANTSIG   0x0002
#define PK_KITE_REJECTED  0x0003
#define PK_KITE_DUPLICATE 0x0004
#define PK_KITE_INVALID   0x0005
#define PK_SALT_LENGTH    36
struct pk_kite_request {
  PK_MEMORY_CANARY
  struct pk_pagekite* kite;
  char bsalt[PK_SALT_LENGTH+1];
  char fsalt[PK_SALT_LENGTH+1];
  int status;
};

/* Data structure describing a frame */
struct pk_frame {
  PK_MEMORY_CANARY
  ssize_t length;             /* Length of data                    */
  char*   data;               /* Pointer to beginning of data      */
  ssize_t hdr_length;         /* Length of header (including nuls) */
  ssize_t raw_length;         /* Length of raw data                */
  char* raw_frame;            /* Raw data (including frame header  */
};

/* Data structure describing a parsed chunk */
#define PK_MAX_CHUNK_HEADERS 64
struct pk_chunk {
  PK_MEMORY_CANARY
  int             header_count;    /* Raw header data, number of headers.    */
  char*           headers[PK_MAX_CHUNK_HEADERS]; /* Raw header data.         */
  char*           sid;             /* SID:   Stream ID                       */
  char*           eof;             /* EOF:   End of stream (r, w or both)    */
  char*           noop;            /* NOOP:  Signal to ignore chunk data     */
  char*           ping;            /* PING:  Request for traffic (keepalive) */
  char*           request_host;    /* Host:  Requested host/domain-name      */
  char*           request_proto;   /* Proto: Requested protocol              */
  int             request_port;    /* Port:  Requested port number           */
  char*           remote_ip;       /* RIP:   Remote IP address (string)      */
  int             remote_port;     /* RPort: Remote port number              */
  char*           remote_tls;      /* RTLS:  Remote TLS encryption (string)  */
  ssize_t         remote_sent_kb;  /* SKB:   Flow control v2                 */
  int             throttle_spd;    /* SPD:   Flow control v1                 */
  int             quota_days;      /* QDays: Quota, days left                */
  int             quota_conns;     /* QConns: Quota, connections left        */
  int             quota_mb;        /* Quota: Quota, megabytes left           */
  int             first_chunk:1;   /* Is this the first chunk of data?       */
  ssize_t         length;          /* Length of available chunk data.        */
  ssize_t         total;           /* Length of total chunk data.            */
  ssize_t         offset;          /* Offset of current fragment.            */
  char*           data;            /* Pointer to chunk data.                 */
  struct pk_frame frame;           /* The raw data.                          */
};

/* Callback for when a chunk is ready. */
typedef void(pkChunkCallback)(void *, struct pk_chunk *);

/* Parser object. */
struct pk_parser {
  PK_MEMORY_CANARY
  int              buffer_bytes_left;
  struct pk_chunk* chunk;
  pkChunkCallback* chunk_callback;
  void*            chunk_callback_data;
};

/* Forward declaration to help us out a bit... */
struct pk_backend_conn;

struct pk_parser* pk_parser_init (int, char*,
                                  pkChunkCallback*, void *);
int               pk_parser_parse(struct pk_parser*, int, char*);
void              pk_parser_reset(struct pk_parser*);
void              pk_chunk_reset(struct pk_chunk*);
void              pk_chunk_reset_values(struct pk_chunk*);

void              pk_reset_pagekite(struct pk_pagekite* kite);

size_t            pk_format_frame(char*, const char*, const char *, size_t);
size_t            pk_reply_overhead(const char *sid, size_t);
size_t            pk_format_reply(char*, const char*, size_t, const char*);
ssize_t           pk_format_chunk(char*, size_t, struct pk_chunk*);
size_t            pk_format_skb(char*, const char*, int);
size_t            pk_format_eof(char*, const char*, int);
size_t            pk_format_pong(char*);
size_t            pk_format_ping(char*);
size_t            pk_format_http_rejection(char*, int, const char*,
                                           const char*, const char*);

int               pk_make_salt(char*);
char*             pk_sign(const char*, const char*, time_t, const char*, int, char *);
char*             pk_prepare_kite_challenge(char *, struct pk_kite_request*, char*, time_t);
int               pk_sign_kite_request(char *, struct pk_kite_request*, int);
char*             pk_parse_kite_request(struct pk_kite_request*, char**, const char*);
struct pk_kite_request* pk_parse_pagekite_response(char*, size_t, char*, char*);
int               pk_connect_ai(struct pk_conn*, struct addrinfo*, int,
                                unsigned int, struct pk_kite_request*, char*,
                                SSL_CTX*, const char*);
int               pk_connect(struct pk_conn*, char*, int,
                             unsigned int, struct pk_kite_request*, char*,
                             SSL_CTX*);
int               pk_http_forwarding_headers_hook(struct pk_chunk*,
                                                  struct pk_backend_conn*);

int pkproto_test(void);
