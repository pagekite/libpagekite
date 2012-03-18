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

#define IO_BUFFER_KB 8

struct pk_pagekite {
  char* protocol;
  char* auth_secret;
  char* public_domain;
  int   public_port;
  int   local_port;
};

#define FE_STATUS_DOWN 0x0000
#define FE_STATUS_UP   0x0001
struct pk_frontend {
  char*             fe_hostname;
  int               fe_port;
  int               priority;
  int               status;
  int               sockfd;
  time_t            activity;
  int               buffer_bytes_free;
  char              buffer[IO_BUFFER_KB * (1024 + PROTO_OVERHEAD_PER_KB)];
  struct pk_parser* parser;
};

#define BE_STATUS_EOF_READ      0x0001
#define BE_STATUS_EOF_WRITE     0x0002
#define BE_STATUS_EOF_THROTTLED 0x0004
struct pk_backend_conn {
  char                sid[8];
  int                 sockfd;
  int                 status;
  time_t              activity;
  int                 in_buffer_bytes_free;
  char                in_buffer[IO_BUFFER_KB * 1024];
  int                 out_buffer_bytes_free;
  char                out_buffer[IO_BUFFER_KB * 1024];
  struct pk_frontend* frontend;
  struct pk_pagekite* kite;
};

#define MIN_KITE_ALLOC 5
#define MIN_FE_ALLOC   4
#define MIN_CONN_ALLOC 25
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
};

struct pk_manager* pk_manager_init(struct ev_loop*,
                                   int, unsigned char*, int, int, int);
int                pk_add_kite(struct pk_manager*, char*, char*, char*, int, int);
int                pk_add_frontend(struct pk_manager*, char*, int, int);

int                pkmanager_test  (void);

