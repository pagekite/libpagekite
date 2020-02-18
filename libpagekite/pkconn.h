/******************************************************************************
pkconn.h - Connection objects

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

#define BLOCKING_FLUSH 1
#define NON_BLOCKING_FLUSH 0

/* These are the controlling parameters for our flow control - in order
 * to minimize buffer bloat, we want to keep the window relatively small
 * and assume that selecting a nearby front-end will keep this from
 * becoming too big a bottleneck on transfer speeds. */
#define CONN_WINDOW_SIZE_KB_MAXIMUM   384  /* 10Mbit/s at 300ms rtt */
#define CONN_WINDOW_SIZE_KB_INITIAL   128  /* Send up 128KB before 1st ACK */
#define CONN_WINDOW_SIZE_KB_MINIMUM     4  /* Kernels eat at least this */
#define CONN_REPORT_INCREMENT          16

typedef enum {
  FLOW_OP_NONE,
  CONN_TUNNEL_BLOCKED,
  CONN_TUNNEL_UNBLOCKED,
  CONN_DEST_BLOCKED,
  CONN_DEST_UNBLOCKED
} flow_op;

typedef enum {
  CONN_CLEAR_DATA,
#ifdef HAVE_OPENSSL
  CONN_SSL_DATA,
  CONN_SSL_HANDSHAKE,
#endif
} io_state_t;

#define CONN_IO_BUFFER_SIZE     PARSER_BYTES_MAX
#define CONN_STATUS_BITS        0x0000FFFF
#define CONN_STATUS_UNKNOWN     0x00000000
#define CONN_STATUS_END_READ    0x00000001 /* Don't want more data     */
#define CONN_STATUS_END_WRITE   0x00000002 /* Won't receive more data  */
#define CONN_STATUS_DST_BLOCKED 0x00000004 /* Destination blocked      */
#define CONN_STATUS_TNL_BLOCKED 0x00000008 /* Tunnel blocked           */
#define CONN_STATUS_BLOCKED    (0x00000008|0x00000004) /* Blocked      */
#define CONN_STATUS_CLS_READ    0x00000010 /* No more data available   */
#define CONN_STATUS_CLS_WRITE   0x00000020 /* No more writing possible */
#define CONN_STATUS_BROKEN     (0x00000040|0x10|0x20) /* ... broken.   */
#define CONN_STATUS_ALLOCATED   0x00000080
#define CONN_STATUS_WANT_READ   0x00000100 /* Want null reads when available  */
#define CONN_STATUS_WANT_WRITE  0x00000200 /* Want null writes when available */
#define CONN_STATUS_LISTENING   0x00000400 /* Listening socket */
#define CONN_STATUS_CHANGING    0x00000800 /* This conn is being changed */
#define PKC_OUT(c)      ((c).out_buffer + (c).out_buffer_pos)
#define PKC_OUT_FREE(c) (CONN_IO_BUFFER_SIZE - (c).out_buffer_pos)
#define PKC_IN(c)       ((c).in_buffer + (c).in_buffer_pos)
#define PKC_IN_FREE(c)  (CONN_IO_BUFFER_SIZE - (c).in_buffer_pos)
struct pk_conn {
  PK_MEMORY_CANARY
  int        status;
  int        sockfd;
  time_t     activity;
  /* Data we have read, vs. what has been sent to remote end. */
  size_t     read_bytes;
  size_t     read_kb;
  size_t     sent_kb;
  size_t     send_window_kb;
  /* Data we have written locally, what we've reported to tunnel. */
  size_t     wrote_bytes;
  size_t     reported_kb;
  /* Buffers, events */
  int        in_buffer_pos;
  char       in_buffer[CONN_IO_BUFFER_SIZE];
  int        out_buffer_pos;
  char       out_buffer[CONN_IO_BUFFER_SIZE];
  ev_io      watch_r;
  ev_io      watch_w;
  io_state_t state;
#ifdef HAVE_OPENSSL
  SSL*       ssl;
  int        want_write;
#endif
};

void    pkc_reset_conn(struct pk_conn*, unsigned int);
int     pkc_connect(struct pk_conn*, struct addrinfo*);
int     pkc_listen(struct pk_conn*, struct addrinfo*, int);
#ifdef HAVE_OPENSSL
int     pkc_start_ssl(struct pk_conn*, SSL_CTX*, const char* hostname);
#endif
int     pkc_wait(struct pk_conn*, int);
ssize_t pkc_read(struct pk_conn*);
int     pkc_pending(struct pk_conn*);
ssize_t pkc_raw_write(struct pk_conn*, char*, ssize_t);
ssize_t pkc_flush(struct pk_conn*, char*, ssize_t, int, char*);
ssize_t pkc_write(struct pk_conn*, char*, ssize_t);
void    pkc_report_progress(struct pk_conn*, char*, struct pk_conn*);

