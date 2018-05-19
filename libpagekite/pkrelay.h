/******************************************************************************
pkrelay.h - Logic specific to front-end relays

*******************************************************************************

This file is Copyright 2011-2017, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify it under
the terms of the  GNU Affero General Public License, version 3.0 or above, as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the Apache License for more details.

You should have received a copy of the GNU AGPL along with this program.
If not, see: <http://www.gnu.org/licenses/agpl.html>

Note: For alternate license terms, see the file COPYING.md.

******************************************************************************/

typedef enum {
  PKR_PROTO_NONE = 0,          /* Invalid protocol, matches nothing */
  PKR_PROTO_PAGEKITE = 1,      /* PageKite protocol */
  PKR_PROTO_HTTP = 2,          /* HTTP kites */
  PKR_PROTO_HTTP_CONNECT = 3,  /* Raw TCP, HTTP proxied */
  PKR_PROTO_RAW = 4,           /* Raw TCP */
  PKR_PROTO_TLS = 5,           /* End-to-end HTTPS */
  PKR_PROTO_LEGACY_SSL = 6,    /* Old fashioned SSL */
  PKR_PROTO_PAGEKITE_PING = 7, /* PageKite PING */
  PKR_PROTO_ANY = 8,           /* Wildcard protocol, matches everything */
  PKR_PROTO_MAX = 9,           /* How many protos exist? */
} pkr_proto_enum;

struct pkr_port_proto_pointer {
  pkr_proto_enum proto;
  int port;
  struct pk_tunnel* tunnel;
};

struct pk_relay_manager {
  int             flying_kites;
  char            global_secret[PK_SALT_LENGTH + 1];
  char**          kite_index;
  pthread_mutex_t kite_index_lock;
  int             kite_index_readers;
  pthread_mutex_t kite_index_readlock;
  pthread_mutex_t kite_index_readblock;
  char*           auth_backend;
};

int pkr_relay_manager_init(struct pk_manager*);
int pkr_relay_manager_free(struct pk_manager*);
struct pk_tunnel* pkr_allocate_more_tunnels(struct pk_manager*);
void pkr_relay_incoming(struct pk_blocker*, int, void*);
int pkrelay_test(void);

#define PKRM(pkm) ((struct pk_relay_manager*) pkm->relay)
