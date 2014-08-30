/******************************************************************************
pkmanager.h - A manager for multiple pagekite connections.

This file is Copyright 2011-2014, The Beanstalks Project ehf.

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

#define PK_HOUSEKEEPING_INTERVAL_MIN    15  /* Seconds */
#define PK_HOUSEKEEPING_INTERVAL_MAX   900  /* 15 minutes */
#define PK_CHECK_WORLD_INTERVAL       3600  /* 1 hour */
#define PK_DDNS_UPDATE_INTERVAL_MIN    360  /* Less than 300 makes no sense,
                                               due to DNS caching TTLs. */

struct pk_tunnel;
struct pk_backend_conn;
struct pk_manager;
struct pk_job;
struct pk_job_pile;

/* These are also written to the conn.status field, using the fourth byte. */
#define FE_STATUS_BITS      0xFF000000
#define FE_STATUS_AUTO      0x00000000  /* For use in pkm_add_tunnel     */
#define FE_STATUS_WANTED    0x01000000  /* Algorithm chose this FE         */
#define FE_STATUS_NAILED_UP 0x02000000  /* User chose this FE              */
#define FE_STATUS_IN_DNS    0x04000000  /* This FE is in DNS               */
#define FE_STATUS_REJECTED  0x08000000  /* Front-end rejected connection   */
#define FE_STATUS_LAME      0x10000000  /* Front-end is going offline      */
#define FE_STATUS_IS_FAST   0x20000000  /* This is a fast front-end        */
struct pk_tunnel {
  /* These apply to frontend connections only */
  char*                   fe_hostname;
  int                     fe_port;
  time_t                  last_ddnsup;
  /* These apply to all tunnels (frontend or backend) */
  char                    fe_session[PK_HANDSHAKE_SESSIONID_MAX];
  struct addrinfo*        ai;
  int                     priority;
  time_t                  last_ping;
  int                     error_count;
  struct pk_conn          conn;
  struct pk_parser*       parser;
  struct pk_manager*      manager;
  int                     request_count;
  struct pk_kite_request* requests;
};

/* These are also written to the conn.status field, using the third byte. */
#define BE_STATUS_BITS           0x00FF0000
#define BE_STATUS_EOF_READ       0x00010000
#define BE_STATUS_EOF_WRITE      0x00020000
#define BE_STATUS_EOF_THROTTLED  0x00040000
#define BE_MAX_SID_SIZE          8
struct pk_backend_conn {
  char                sid[BE_MAX_SID_SIZE];
  struct pk_tunnel* tunnel;
  struct pk_pagekite* kite;
  struct pk_conn      conn;
};

#define MIN_KITE_ALLOC        4
#define MIN_FE_ALLOC          2
#define MIN_CONN_ALLOC       16
#define MAX_BLOCKING_THREADS 16
#define PK_MANAGER_BUFSIZE(k, f, c, ps) \
                           (1 + sizeof(struct pk_manager) \
                            + sizeof(struct pk_pagekite) * k \
                            + sizeof(struct pk_tunnel) * f \
                            + sizeof(struct pk_kite_request) * f * k \
                            + ps * f \
                            + sizeof(struct pk_backend_conn) * c \
                            + sizeof(struct pk_job) * (c+f))
#define PK_MANAGER_MINSIZE PK_MANAGER_BUFSIZE(MIN_KITE_ALLOC, MIN_FE_ALLOC, \
                                              MIN_CONN_ALLOC, PARSER_BYTES_MIN)

struct pk_manager {
  pk_status_t              status;

  int                      buffer_bytes_free;
  char*                    buffer;
  char*                    buffer_base;
  struct pk_pagekite*      kites;
  struct pk_tunnel*        tunnels;
  struct pk_backend_conn*  be_conns;

  pthread_t                main_thread;
  pthread_mutex_t          loop_lock;
  struct ev_loop*          loop;
  ev_async                 interrupt;
  ev_async                 quit;
  ev_async                 tick;
  ev_timer                 timer;

  time_t                   last_world_update;
  time_t                   next_tick;
  unsigned int             enable_timer:1;
  time_t                   last_dns_update;

  SSL_CTX*                 ssl_ctx;
  pthread_t                watchdog_thread;
  pthread_t*               blocking_threads[MAX_BLOCKING_THREADS];
  struct pk_job_pile       blocking_jobs;

  /* Settings */
  int                      kite_max;
  int                      tunnel_max;
  int                      be_conn_max;
  unsigned int             fancy_pagekite_net_rejection:1;
  unsigned int             enable_watchdog:1;
  int                      want_spare_frontends;
  char*                    dynamic_dns_url;
  time_t                   interval_fudge_factor;
  time_t                   housekeeping_interval_min;
  time_t                   housekeeping_interval_max;
  time_t                   check_world_interval;
};


void* pkm_run                       (void *);
int pkm_run_in_thread               (struct pk_manager*);
int pkm_wait_thread                 (struct pk_manager*);
int pkm_stop_thread                 (struct pk_manager*);
int pkm_reconnect_all               (struct pk_manager*);
int pkm_disconnect_unused           (struct pk_manager*);

struct pk_manager*   pkm_manager_init(struct ev_loop*,
                                      int, char*, int, int, int,
                                      const char*, SSL_CTX*);
void                 pkm_reset_timer(struct pk_manager*);
void                 pkm_set_timer_enabled(struct pk_manager*, int);
void                 pkm_quit(struct pk_manager*);
void                 pkm_tick(struct pk_manager*);
struct pk_pagekite*  pkm_add_kite(struct pk_manager*,
                                  const char*, const char*, int, const char*,
                                  const char*, int);
struct pk_pagekite*  pkm_find_kite(struct pk_manager*,
                                   const char*, const char*, int);
ssize_t              pkm_write_chunked(struct pk_tunnel*,
                                       struct pk_backend_conn*,
                                       ssize_t, char*);
int                  pkm_add_frontend(struct pk_manager*,
                                      const char*, int, int);
struct pk_tunnel*    pkm_add_frontend_ai(struct pk_manager*, struct addrinfo*,
                                         const char*, int, int);
void                 pkm_reset_conn(struct pk_conn*);

ssize_t              pkm_write_data(struct pk_conn*, ssize_t, char*);
ssize_t              pkm_read_data(struct pk_conn*);
ssize_t              pkm_flush(struct pk_conn*, char*, ssize_t, int, char*);
void                 pkm_parse_eof(struct pk_backend_conn*, char*);
int                  pkm_update_io(struct pk_tunnel*, struct pk_backend_conn*);
void                 pkm_flow_control_tunnel(struct pk_tunnel*, flow_op);
void                 pkm_flow_control_conn(struct pk_conn*, flow_op);

/* Backend connection handling */
struct pk_backend_conn*  pkm_connect_be(struct pk_tunnel*, struct pk_chunk*);
struct pk_backend_conn*  pkm_alloc_be_conn(struct pk_manager*,
                                           struct pk_tunnel*, char*);
struct pk_backend_conn*  pkm_find_be_conn(struct pk_manager*,
                                          struct pk_tunnel*,  char*);
void                     pkm_free_be_conn(struct pk_backend_conn*);

void pkm_chunk_cb(struct pk_tunnel*, struct pk_chunk *);
void pkm_tunnel_readable_cb(EV_P_ ev_io *, int);
void pkm_tunnel_writable_cb(EV_P_ ev_io *, int);
void pkm_be_conn_readable_cb(EV_P_ ev_io *, int);
void pkm_be_conn_writable_cb(EV_P_ ev_io *, int);
void pkm_timer_cb(EV_P_ ev_timer *w, int);
