/******************************************************************************
pktunnel.h - Common tunnel definitions and functions

This file is Copyright 2011-2017, The Beanstalks Project ehf.

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

struct pk_tunnel;
struct pk_backend_conn;
struct pk_manager;

/* These are also written to the conn.status field, using the fourth byte. */
#define FE_STATUS_BITS      0xFF000000
#define FE_STATUS_AUTO      0x00000000  /* For use in pkm_add_tunnel       */
#define FE_STATUS_WANTED    0x01000000  /* Algorithm chose this FE         */
#define FE_STATUS_NAILED_UP 0x02000000  /* User chose this FE              */
#define FE_STATUS_IN_DNS    0x04000000  /* This FE is in DNS               */
#define FE_STATUS_REJECTED  0x08000000  /* Front-end rejected connection   */
#define FE_STATUS_LAME      0x10000000  /* Front-end is going offline      */
#define FE_STATUS_IS_FAST   0x20000000  /* This is a fast front-end        */
#define BE_VERSION_MAXLEN   12
struct pk_tunnel_fe {
  /* These apply to connections to a frontend (on the backend) */
  char*                   hostname;
  int                     port;
  struct addrinfo         ai;
  int                     priority;
  time_t                  last_ddnsup;
  time_t                  last_configured;
  int                     request_count;
  struct pk_kite_request* requests;
};
#if HAVE_RELAY
struct pk_tunnel_be {
  /* These apply to connections to a backend (on the frontend relay) */
  char                    be_version[PK_HANDSHAKE_VERSION_MAX+1];
  pagekite_callback_t*    cleanup_callback_func;
};
#endif
struct pk_tunnel {
  PK_MEMORY_CANARY
  /* These apply to all tunnels (frontend or backend) */
  struct pk_manager*      manager;
  struct pk_parser*       parser;
  struct pk_backend_conn* first_pkb;
  struct pk_conn          conn;
  time_t                  last_ping;
  char                    session_id[PK_HANDSHAKE_SESSIONID_MAX+1];
  int                     error_count:32;
  int                     remote_is_be:1;
  /* These are the per-role data. We use a union to save bytes. */
  union {
    struct pk_tunnel_fe   fe;
#if HAVE_RELAY
    struct pk_tunnel_be   be;
#endif
  }                       remote;
  /* This has to be outside the union, because it's allocated up front */
  struct pk_kite_request* requests;
};

/* These are also written to the conn.status field, using the third byte. */
#define BE_STATUS_BITS           0x00FF0000
#define BE_STATUS_EOF_READ       0x00010000
#define BE_STATUS_EOF_WRITE      0x00020000
#define BE_STATUS_EOF_THROTTLED  0x00040000
struct pk_backend_conn {
  PK_MEMORY_CANARY
  char                    sid[PK_MAX_SID_SIZE+1];
  struct pk_tunnel*       tunnel;
  struct pk_pagekite*     kite;
  struct pk_conn          conn;
  struct pk_backend_conn* next_pkb;
  pagekite_callback2_t*   accept_callback_func;
  void*                   accept_callback_data;
};


int pkt_setup_tunnel(struct pk_manager*,
                     struct pk_tunnel*,
                     struct pk_backend_conn*);
int pktunnel_test(void);
