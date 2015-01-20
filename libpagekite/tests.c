/******************************************************************************
tests.c - Unit tests for pagekite.

This file is Copyright 2011-2015, The Beanstalks Project ehf.

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
#include <assert.h>

#include "pkutils.h"
#include "pkerror.h"
#include "pkconn.h"
#include "pkstate.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"
#include "pkwatchdog.h"

struct pk_global_state pk_state;

int sha1_test();
int utils_test();
int pkproto_test();
int pkmanager_test();

int main(void) {
#ifdef _MSC_VER
  pks_global_init(PK_LOG_ALL);

  /* Initialize Winsock */
  int r;
  WSADATA wsa_data;
  r = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (r != 0) {
    fprintf(stderr, "WSAStartup failed: %d\n", r);
    return 1;
  }
#endif
  assert(sha1_test());
  assert(utils_test());
  assert(pkproto_test());
  assert(pkmanager_test());
  return 0;
}


