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
#include "pkproto.h"
#include "pkstate.h"
#include "pktunnel.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"
#include "pkerror.h"



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
