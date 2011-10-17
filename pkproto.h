/******************************************************************************
pkproto.h - A basic serializer/deserializer for the PageKite tunnel protocol.

This file is Copyright 2011, The Beanstalks Project ehf.

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

******************************************************************************/
#define PARSE_BAD_FRAME -10000
#define PARSE_BAD_CHUNK -10001

/* Data structure describing a frame */
struct pk_frame {
  int   length;                /* Length of data                    */
  char* data;                  /* Pointer to beginning of data      */
  int   hdr_length;            /* Length of header (including nuls) */
  int   raw_length;            /* Length of raw data                */
  char* raw_frame;             /* Raw data (including frame header  */
};

/* Data structure describing a parsed chunk */
struct pk_chunk {
  char*           sid;             /* SID:   Stream ID                       */
  char*           eof;             /* EOF:   End of stream (r, w or both)    */
  char*           noop;            /* NOOP:  Signal to ignore chunk data     */
  char*           ping;            /* PING:  Request for traffic (keepalive) */
  char*           request_host;    /* Host:  Requested host/domain-name      */
  char*           request_proto;   /* Proto: Requested protocol              */
  int             request_port;    /* Port:  Requested port number (string)  */
  char*           remote_ip;       /* RIP:   Remote IP address (string)      */
  int             remote_port;     /* RPort: Remote port number (string)     */
  int             throttle_spd;    /* SPD:   Flow control                    */
  int             length;          /* Length of chunk data.                  */
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

struct pk_parser* parser_create (int, char*,
                                 pkChunkCallback*, void *);
int               parser_parse  (struct pk_parser*, int, char*);

int               pkproto_test  (void);

