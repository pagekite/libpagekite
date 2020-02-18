/******************************************************************************
pkmanager.h - A manager for multiple pagekite connections.

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

#define PK_HOUSEKEEPING_INTERVAL_MIN       15 /* Seconds */
#define PK_HOUSEKEEPING_INTERVAL_MAX_MIN  120 /* 2 minutes: lower is bad! */
#define PK_HOUSEKEEPING_INTERVAL_MAX_DEF  300 /* 5 minutes */
#define PK_CHECK_WORLD_INTERVAL          3600 /* 1 hour */
#define PK_DDNS_UPDATE_INTERVAL_MIN       360 /* Less than 300 makes no sense,
                                                 due to DNS caching TTLs. */

struct pk_tunnel;
struct pk_backend_conn;
struct pk_manager;
struct pk_job;
struct pk_job_pile;

/* These are also written to the conn.status field, using the fourth byte. */
#define FE_STATUS_BITS      0xFF000000
#define FE_STATUS_AUTO      0x00000000  /* For use in pkm_add_tunnel       */
#define FE_STATUS_WANTED    0x01000000  /* Algorithm chose this FE         */
#define FE_STATUS_NAILED_UP 0x02000000  /* User chose this FE              */
#define FE_STATUS_IN_DNS    0x04000000  /* This FE is in DNS               */
#define FE_STATUS_REJECTED  0x08000000  /* Front-end rejected connection   */
#define FE_STATUS_LAME      0x10000000  /* Front-end is going offline      */
#define FE_STATUS_IS_FAST   0x20000000  /* This is a fast front-end        */
struct pk_tunnel {
  PK_MEMORY_CANARY
  /* These apply to frontend connections only (on the backend) */
  char*                   fe_hostname;
  int                     fe_port;
  time_t                  last_ddnsup;
  int                     priority;
  char*                   fe_uuid;
  /* These apply to all tunnels (frontend or backend) */
  struct addrinfo         ai;
  struct pk_conn          conn;
  int                     error_count;
  char                    fe_session[PK_HANDSHAKE_SESSIONID_MAX+1];
  time_t                  last_ping;
  time_t                  last_configured;
  struct pk_manager*      manager;
  struct pk_parser*       parser;
  int                     request_count;
  struct pk_kite_request* requests;
  pagekite_callback_t*    callback_func;
  void*                   callback_data;
};

/* These are also written to the conn.status field, using the third byte. */
#define BE_STATUS_BITS           0x00FF0000
#define BE_STATUS_EOF_READ       0x00010000
#define BE_STATUS_EOF_WRITE      0x00020000
#define BE_STATUS_EOF_THROTTLED  0x00040000
#define BE_MAX_SID_SIZE          8
struct pk_backend_conn {
  PK_MEMORY_CANARY
  char                 sid[BE_MAX_SID_SIZE+1];
  struct pk_tunnel*    tunnel;
  struct pk_pagekite*  kite;
  struct pk_conn       conn;
  pagekite_callback_t* callback_func;
  void*                callback_data;
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
#define PK_TUNNEL_ITER(pkm, fe) for (struct pk_tunnel* fe = pkm->tunnels; fe < (pkm->tunnels + pkm->tunnel_max); fe++)
#define PK_KITE_ITER(pkm, kite) for (struct pk_pagekite* kite = pkm->kites; kite < (pkm->kites + pkm->kite_max); kite++)

struct pk_manager {
  pk_status_t              status;

  int                      buffer_bytes_free;
  char*                    buffer;
  char*                    buffer_base;
  struct pk_pagekite*      kites;
  struct pk_tunnel*        tunnels;
  struct pk_backend_conn*  be_conns;

  PK_MEMORY_CANARY

  pthread_t                main_thread;
  pthread_mutex_t          config_lock;
  pthread_mutex_t          intr_lock;
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
  struct pk_blocker*       blocking_threads[MAX_BLOCKING_THREADS];
  struct pk_job_pile       blocking_jobs;
  struct pke_events        events;

  /* Settings */
  int                      kite_max;
  int                      tunnel_max;
  int                      be_conn_max;
  unsigned int             was_malloced:1;
  unsigned int             ev_loop_malloced:1;
  unsigned int             enable_watchdog:1;
  unsigned int             enable_http_forwarding_headers:1;
  int                      want_spare_frontends;
  char*                    fancy_pagekite_net_rejection_url;
  char*                    dynamic_dns_url;
  time_t                   interval_fudge_factor;
  time_t                   housekeeping_interval_min;
  time_t                   housekeeping_interval_max;
  time_t                   check_world_interval;
};


struct pk_manager*   pkm_manager_init(struct ev_loop*,
                                      int, char*, int, int, int,
                                      const char*, SSL_CTX*);
void pkm_manager_free(struct pk_manager*);

int                  pkm_add_frontend(struct pk_manager*,
                                      const char*, int, int);
int                  pkm_lookup_and_add_frontend(struct pk_manager*,
                                                 const char*, int, int, int);
struct pk_tunnel*    pkm_add_frontend_ai(struct pk_manager*, struct addrinfo*,
                                         const char*, int, int);

struct pk_pagekite*  pkm_add_kite(struct pk_manager*,
                                  const char*, const char*, int, const char*,
                                  const char*, int);

int                 pkm_add_listener(struct pk_manager*, const char*, int,
                                     pagekite_callback_t*, void*);

struct pk_backend_conn* pkm_alloc_be_conn(struct pk_manager*,
                                          struct pk_tunnel*, char *);
void pkm_free_be_conn(struct pk_backend_conn* pkb);
struct pk_backend_conn* pkm_find_be_conn(struct pk_manager*,
                                         struct pk_tunnel*, char*);


void* pkm_run                       (void *);
int pkm_run_in_thread               (struct pk_manager*);
int pkm_wait_thread                 (struct pk_manager*);
int pkm_stop_thread                 (struct pk_manager*);
int pkm_reconfig_start              (struct pk_manager*);
void pkm_reconfig_stop              (struct pk_manager*);

int pkm_reconnect_all               (struct pk_manager*, int);
int pkm_disconnect_unused           (struct pk_manager*);

void pkm_set_timer_enabled          (struct pk_manager*, int);
void pkm_tick                       (struct pk_manager*);

int pkmanager_test(void);
