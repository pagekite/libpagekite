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
struct pk_tunnel {
  PK_MEMORY_CANARY
  /* These apply to frontend connections only (on the backend) */
  char*                   fe_hostname;
  int                     fe_port;
  time_t                  last_ddnsup;
  int                     priority;
#if HAVE_RELAY
  /* These apply to backend connections only (on the frontend) */
  char                    be_version[PK_HANDSHAKE_VERSION_MAX+1];
#endif
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
struct pk_backend_conn {
  PK_MEMORY_CANARY
  char                 sid[PK_MAX_SID_SIZE+1];
  struct pk_tunnel*    tunnel;
  struct pk_pagekite*  kite;
  struct pk_conn       conn;
  pagekite_callback_t* callback_func;
  void*                callback_data;
};


int pktunnel_test(void);
