/******************************************************************************
pkrelay.c - Logic specific to front-end relays

*******************************************************************************

This file is Copyright 2011-2015, The Beanstalks Project ehf.

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

#include "pagekite.h"

#include "common.h"
#include "utils.h"
#include "pkstate.h"
#include "pkerror.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pkrelay.h"
#include "pklogging.h"


static void pkr_new_conn_readable_cb(EV_P_ ev_io* w, int revents)
{
  struct pk_backend_conn* pkb = (struct pk_backend_conn*) w->data;
  
  PK_TRACE_FUNCTION;
  
  /* We have data: peek() at it to find out what it is!
   *   - Is this TLS?
   *       - Is it a connection to a remote tunneled backend?
   *       - Is it a connection to one of our own certs?
   *       - None of the above: eat the data, return an error
   *   - Is this HTTP?
   *       - Is it a connection to a remote tunneled backend?
   *       - Is it a connection to an internal LUA HTTPd?
   *       - Is it a PageKite tunnel request?
   *       - None of the above: eat the data, return an error
   *   - Does this belong to a LUA plugin?
   *   - Need more data? Wait...
   *   - Else: close silently
   */

  PK_CHECK_MEMORY_CANARIES;

  /* -Wall dislikes unused arguments */
  (void) loop;
  (void) revents;
}


/* This is the public library method; we define it here instead of in
 * pagekite.c to simplify building the library without AGPLv3 code.
 */
int pagekite_add_relay_listener(pagekite_mgr pkm, int port)
{
  if (pkm == NULL) return -1;

  struct pk_manager* m = (struct pk_manager*) pkm;
  return -1;
}

