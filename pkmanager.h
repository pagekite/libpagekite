/******************************************************************************
pkmanager.h - A manager for multiple pagekite connections.

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

******************************************************************************/

#define PARSER_BYTES_MIN   1 * 1024
#define PARSER_BYTES_AVG   2 * 1024
#define PARSER_BYTES_MAX   4 * 1024  /* <= CONN_IO_BUFFER_SIZE */

#define BLOCKING_FLUSH 1
#define NON_BLOCKING_FLUSH 0

#define PK_HOUSEKEEPING_INTERVAL 10  /* Seconds */

struct pk_conn;
struct pk_frontend;
struct pk_backend_conn;
struct pk_manager;

#define CONN_IO_BUFFER_SIZE  4 * 1024
#define CONN_STATUS_UNKNOWN    0x0000
#define CONN_STATUS_END_READ   0x0001 /* Don't want more data     */
#define CONN_STATUS_END_WRITE  0x0002 /* Won't receive more data  */
#define CONN_STATUS_THROTTLED  0x0004 /* Destination blocked      */
#define CONN_STATUS_CLS_READ   0x0010 /* No more data available   */
#define CONN_STATUS_CLS_WRITE  0x0020 /* No more writing possible */
#define CONN_STATUS_BROKEN     0x0130 /* Socket is defunct        */
#define CONN_STATUS_ALLOCATED  0x1000
#define PKC_OUT(c)      ((c).out_buffer + (c).out_buffer_pos)
#define PKC_OUT_FREE(c) (CONN_IO_BUFFER_SIZE - (c).out_buffer_pos)
#define PKC_IN(c)       ((c).in_buffer + (c).in_buffer_pos)
#define PKC_IN_FREE(c)  (CONN_IO_BUFFER_SIZE - (c).in_buffer_pos)
struct pk_conn {
  int      status;
  int      sockfd;
  time_t   activity;
  int      in_buffer_pos;
  char     in_buffer[CONN_IO_BUFFER_SIZE];
  int      out_buffer_pos;
  char     out_buffer[CONN_IO_BUFFER_SIZE];
  ev_io    watch_r;
  ev_io    watch_w;
};

#define FE_STATUS_DOWN  0x0010
#define FE_STATUS_UP    0x0020
struct pk_frontend {
  char*                   fe_hostname;
  int                     fe_port;
  int                     priority;
  struct pk_conn          conn;
  struct pk_parser*       parser;
  struct pk_manager*      manager;
  int                     request_count;
  struct pk_kite_request* requests;
};

#define BE_STATUS_EOF_READ       0x0100
#define BE_STATUS_EOF_WRITE      0x0200
#define BE_STATUS_EOF_THROTTLED  0x0400
#define BE_MAX_SID_SIZE          8
struct pk_backend_conn {
  char                sid[BE_MAX_SID_SIZE];
  struct pk_frontend* frontend;
  struct pk_pagekite* kite;
  struct pk_conn      conn;
};

#define MIN_KITE_ALLOC   4
#define MIN_FE_ALLOC     2
#define MIN_CONN_ALLOC  16
#define PK_MANAGER_BUFSIZE(k, f, c, ps) \
                           (sizeof(struct pk_manager) + \
                            sizeof(struct pk_pagekite) * k + \
                            sizeof(struct pk_frontend) * f + \
                            sizeof(struct pk_kite_request) * f * k + \
                            ps * f + \
                            sizeof(struct pk_backend_conn) * c + 1)
#define PK_MANAGER_MINSIZE PK_MANAGER_BUFSIZE(MIN_KITE_ALLOC, MIN_FE_ALLOC, \
                                              MIN_CONN_ALLOC, PARSER_BYTES_MIN)
struct pk_manager {
  int                      kite_count;
  struct pk_pagekite*      kites;
  int                      frontend_count;
  struct pk_frontend*      frontends;
  int                      be_conn_count;
  struct pk_backend_conn*  be_conns;
  int                      buffer_bytes_free;
  char*                    buffer;
  char*                    buffer_base;
  struct ev_loop*          loop;
  ev_timer                 timer;
};

struct pk_manager*   pkm_manager_init(struct ev_loop*,
                                      int, char*, int, int, int);
struct pk_pagekite*  pkm_add_kite(struct pk_manager*,
                                  const char*, const char*, int, const char*,
                                  const char*, int);
struct pk_pagekite*  pkm_find_kite(struct pk_manager*,
                                   const char*, const char*, int);
ssize_t              pkm_write_chunked(struct pk_frontend*,
                                       struct pk_backend_conn*,
                                       ssize_t, char*);
struct pk_frontend*  pkm_add_frontend(struct pk_manager*,
                                      const char*, int, int);
void                 pkm_reset_conn(struct pk_conn*);

ssize_t              pkm_write_data(struct pk_conn*, ssize_t, char*);
ssize_t              pkm_read_data(struct pk_conn*);
ssize_t              pkm_flush(struct pk_conn*, char*, ssize_t, int);
void                 pkm_parse_eof(struct pk_backend_conn*, char*);
int                  pkm_update_io(struct pk_frontend*, struct pk_backend_conn*);


/* Backend connection handling */
struct pk_backend_conn*  pkm_connect_be(struct pk_frontend*, struct pk_chunk*);
struct pk_backend_conn*  pkm_alloc_be_conn(struct pk_manager*, char*);
struct pk_backend_conn*  pkm_find_be_conn(struct pk_manager*, char*);
void                     pkm_free_be_conn(struct pk_backend_conn*);


void pkm_chunk_cb(struct pk_frontend*, struct pk_chunk *);
void pkm_tunnel_readable_cb(EV_P_ ev_io *, int);
void pkm_tunnel_writable_cb(EV_P_ ev_io *, int);
void pkm_be_conn_readable_cb(EV_P_ ev_io *, int);
void pkm_be_conn_writable_cb(EV_P_ ev_io *, int);
void pkm_timer_cb(EV_P_ ev_timer *w, int);

