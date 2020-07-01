/******************************************************************************
pkstate.h - Global program state

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

#define PKS_LOG_DATA_MAX     64*1024

typedef enum {
  PK_ENUM_STATUS_STARTUP      = PK_STATUS_STARTUP,
  PK_ENUM_STATUS_CONNECTING   = PK_STATUS_CONNECTING,
  PK_ENUM_STATUS_UPDATING_DNS = PK_STATUS_UPDATING_DNS,
  PK_ENUM_STATUS_FLYING       = PK_STATUS_FLYING,
  PK_ENUM_STATUS_PROBLEMS     = PK_STATUS_PROBLEMS,
  PK_ENUM_STATUS_REJECTED     = PK_STATUS_REJECTED,
  PK_ENUM_STATUS_NO_NETWORK   = PK_STATUS_NO_NETWORK
} pk_status_t;

struct pk_global_state {
  /* Synchronization */
  pthread_mutex_t lock;
  pthread_cond_t  cond;

  /* Global logging state */
  FILE*           log_file;
  unsigned int    log_mask;
  char            log_ring_buffer[PKS_LOG_DATA_MAX+1];
  char*           log_ring_start;
  char*           log_ring_end;

  /* Settings */
  unsigned int    bail_on_errors;
  time_t          conn_eviction_idle_s;
  time_t          socket_timeout_s;
  unsigned int    fake_ping:1;
  char*           ssl_ciphers;
  char**          ssl_cert_names;
  unsigned int    use_ipv4:1;
#ifdef HAVE_IPV6
  unsigned int    use_ipv6:1;
#endif
  unsigned int    ddns_request_public_ipv4:1;
  unsigned int    ddns_request_public_ipv6:1;
  char*           dns_check_name;

  /* Global program state */
  unsigned int    live_streams;
  unsigned int    live_tunnels;
  unsigned int    live_listeners;
  unsigned int    have_ssl:1;
  unsigned int    force_update:1;
  char*           app_id_short;
  char*           app_id_long;

  /* Quota state (assuming frontends agree) */
  int             quota_days;
  int             quota_conns;
  int             quota_mb;
};

#ifndef __IN_PKSTATE_C__
extern struct pk_global_state pk_state;
#else
struct pk_global_state pk_state;
#endif

#define PKS_STATE(change) { pthread_mutex_lock(&(pk_state.lock)); \
                            change; \
                            PK_HOOK(PK_HOOK_STATE_CHANGED, 0, &pk_state, NULL); \
                            pthread_cond_broadcast(&(pk_state.cond)); \
                            pthread_mutex_unlock(&(pk_state.lock)); } 
#define PKS_STATE_LOCK    pthread_mutex_lock(&(pk_state.lock)); {
#define PKS_STATE_UNLOCK  } pthread_mutex_unlock(&(pk_state.lock));

void pks_global_init(unsigned int log_level);
int pks_logcopy(const char*, size_t len);
void pks_copylog(char*);
void pks_printlog(FILE *dest);
void pks_free_ssl_cert_names();
void pks_add_ssl_cert_names(char**);
