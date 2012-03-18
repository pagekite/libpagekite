/******************************************************************************
pkmanager.c - A manager for multiple pagekite connections.

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

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <ev.h>

#include "utils.h"
#include "pkproto.h"
#include "pkmanager.h"

struct pk_manager* pk_manager_init(struct ev_loop* loop,
                                   int buffer_size, unsigned char* buffer,
                                   int kites, int frontends, int conns)
{
  struct pk_manager* pkm;
  int i;

  pkm = (struct pk_manager*) buffer;
  pkm->buffer_bytes_free = buffer_size;

  pkm->buffer = buffer + sizeof(struct pk_manager);
  pkm->buffer_bytes_free -= sizeof(struct pk_manager);
  pkm->buffer_base = pkm->buffer;

  i = kites + frontends + conns;

  return pkm;  
}

int pk_add_kite(struct pk_manager* pkm,
                char* protocol, char* auth_secret,
                char* public_domain, int public_port,
                int local_port)
{
  return 1;
}

int pk_add_frontend(struct pk_manager* pkm,
                    char* hostname, int port, int priority)
{
  return 1;
}


/**[ TESTS ]******************************************************************/

int pkmanager_test(void)
{
  unsigned char buffer[64000];
  struct pk_manager* m = pk_manager_init(NULL, 64000, buffer, -1, -1, -1);
  return 1;
}

