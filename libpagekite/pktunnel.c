/******************************************************************************
pktunnel.c - Tunnels!

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

#define PAGEKITE_CONSTANTS_ONLY
#include "pagekite.h"

#include "pkcommon.h"
#include "pkutils.h"
#include "pkconn.h"
#include "pkhooks.h"
#include "pkproto.h"
#include "pkstate.h"
#include "pktunnel.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"
#include "pkerror.h"


void pkt_remove_pkb(struct pk_tunnel* tun, struct pk_backend_conn* pkb)
{
  struct pk_backend_conn** p = &(((tun) ? tun : pkb->tunnel)->first_pkb);
  while (*p != NULL) {
    if (*p == pkb) {
      *p = pkb->next_pkb;
      return;
    }
    p = &((*p)->next_pkb);
  }
}

void pkt_add_pkb(struct pk_tunnel* tun, struct pk_backend_conn* pkb)
{
  pkb->next_pkb = tun->first_pkb;
  tun->first_pkb = pkb;
}

void pkt_cleanup(struct pk_tunnel* tun)
{
  if (tun->remote_is_be) {
    tun->remote.be.cleanup_callback_func = NULL;
  }
  else {
    if (tun->remote.fe.hostname) free(tun->remote.fe.hostname);
    tun->remote.fe.hostname = NULL;
  }
}

void pkt_set_remote_is_be(struct pk_tunnel* tun, int become_be)
{
  if (become_be == tun->remote_is_be) return;
  pkt_cleanup(tun);
  if (become_be) {
    tun->remote_is_be = 1;
    tun->remote.be.cleanup_callback_func = NULL;
  }
  else {
    tun->remote_is_be = 0;
    tun->remote.fe.hostname = NULL;
  }
  assert(become_be == tun->remote_is_be);
}

int pkt_setup_tunnel(struct pk_manager* pkm,
                     struct pk_tunnel* tun,
                     struct pk_backend_conn* pkb)
{
  /* FIXME: Configure tunnel from pkb if necessary */

  pkm_start_tunnel_io(pkm, tun);
  return 0;
}


/* *** Tests *************************************************************** */

#if PK_TESTS
static int pktunnel_test_format_frame(void)
{
  char dest[1024];
  char* expect = "e\r\nSID: 12345\r\n\r\n";
  size_t bytes = strlen(expect);
  assert(bytes == pk_format_frame(dest, "12345", "SID: %s\r\n\r\n", 0));
  assert(0 == strncmp(expect, dest, bytes));
  return 1;
}
#endif

int pktunnel_test(void)
{
#if PK_TESTS
  assert(1 == pktunnel_test_format_frame());
  return 1;
#else
  return 1;
#endif
}
