/******************************************************************************
pkproto.h - A basic serializer/deserializer for the PageKite tunnel protocol.

This file is Copyright 2011, 2012, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify it under
the terms of the  GNU  Affero General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for more
details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see: <http://www.gnu.org/licenses/>

Note: For alternate license terms, see the file COPYING.md.

******************************************************************************/

/* This magic number is a high estimate of how much overhead we expect the
 * PageKite frame and chunk headers to add to each sent packet.
 *
 * 12345Z1234\r\nSID: 123456789\r\n\r\n = 30 bytes, so double that.
 */
#define PROTO_OVERHEAD_PER_KB  64

#define PK_HANDSHAKE_CONNECT ("CONNECT PageKite:1 HTTP/1.0\r\n" \
                              "X-PageKite-Version: c1\r\n")
#ifdef ANDROID
#define PK_HANDSHAKE_FEATURES "X-PageKite-Features: Mobile\r\n"
#else
#define PK_HANDSHAKE_FEATURES ""
#endif
#define PK_HANDSHAKE_KITE "X-PageKite: %s\r\n"
#define PK_HANDSHAKE_END "\r\n"

#define PK_EOF_READ  0x1
#define PK_EOF_WRITE 0x2
#define PK_EOF       (PK_EOF_READ | PK_EOF_WRITE)

/* Data structure describing a kite */
struct pk_pagekite {
  char* protocol;
  char* public_domain;
  int   public_port;
  char* local_domain;
  int   local_port;
  char* auth_secret;
};

/* Data structure describing a kite request */
#define PK_STATUS_UNKNOWN   0x0000
#define PK_STATUS_CONNECTED 0x0001
#define PK_STATUS_REJECTED  0x0002
struct pk_kite_request {
  struct pk_pagekite* kite;
  char* bsalt;
  char* fsalt;
  int status;
};

/* Data structure describing a frame */
struct pk_frame {
  ssize_t length;             /* Length of data                    */
  char*   data;               /* Pointer to beginning of data      */
  ssize_t hdr_length;         /* Length of header (including nuls) */
  ssize_t raw_length;         /* Length of raw data                */
  char* raw_frame;            /* Raw data (including frame header  */
};

/* Data structure describing a parsed chunk */
#define PK_MAX_CHUNK_HEADERS 64
struct pk_chunk {
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
  ssize_t         length;          /* Length of chunk data.                  */
  char*           data;            /* Pointer to chunk data.                 */
  struct pk_frame frame;           /* The raw data.                          */
};

/* Callback for when a chunk is ready. */
typedef void(pkChunkCallback)(void *, struct pk_chunk *);

/* Parser object. */
struct pk_parser {
  int              buffer_bytes_left;
  struct pk_chunk* chunk;
  pkChunkCallback* chunk_callback;
  void*            chunk_callback_data;
};

struct pk_parser* pk_parser_init (int, char*,
                                  pkChunkCallback*, void *);
int               pk_parser_parse(struct pk_parser*, int, char*);
void              pk_parser_reset(struct pk_parser*);

void              pk_reset_pagekite(struct pk_pagekite* kite);

size_t            pk_format_frame(char*, const char*, const char *, size_t);
size_t            pk_reply_overhead(const char *sid, size_t);
size_t            pk_format_reply(char*, const char*, size_t, const char*);
size_t            pk_format_eof(char*, const char*, int);
size_t            pk_format_pong(char*);

int               pk_make_bsalt(struct pk_kite_request*);
int               pk_sign_kite_request(char *, struct pk_kite_request*, int);
char*             pk_parse_kite_request(struct pk_kite_request*, const char*);
int               pk_connect(char *, int, struct sockaddr_in*,
                             unsigned int, struct pk_kite_request*);

