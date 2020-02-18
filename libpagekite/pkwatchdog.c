/******************************************************************************
pkwatchdog.c - Watchdog thread that just dumps core if ignored.

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

#define PAGEKITE_CONSTANTS_ONLY
#include "pagekite.h"

#include "pkcommon.h"
#include "pkutils.h"
#include "pkerror.h"
#include "pkhooks.h"
#include "pkconn.h"
#include "pkstate.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"


time_t pk_global_watchdog_ticker = 0;


void pkw_pet_watchdog()
{
   if (pk_global_watchdog_ticker >= 0)
     pk_global_watchdog_ticker = pk_time();
}

void* pkw_run_watchdog(void *void_pkm)
{
  int i;
  int *segfaulter;
  time_t last_ticker = 0xDEADBEEF;
  struct pk_manager* pkm = (struct pk_manager*) void_pkm;
  pk_log(PK_LOG_MANAGER_DEBUG, "Started watchdog thread.");

  segfaulter = (int*) 1;
  while (1) {
    if (last_ticker == pk_global_watchdog_ticker) {
      pk_log(PK_LOG_MANAGER_DEBUG, "Watchdog is angry! Dumping core.");
      *segfaulter = 0xDEADBEEF;
      assert(0);
    }
    pk_log(PK_LOG_MANAGER_DEBUG, "Watchdog is content, sleeping.");
    last_ticker = pk_global_watchdog_ticker;
    for (i = 0; i < (pkm->housekeeping_interval_max * 2); i++) {
      if (pk_global_watchdog_ticker < 0) return NULL;
      sleep(1);
    }
  }
}

int pkw_start_watchdog(struct pk_manager *pkm)
{
  pk_global_watchdog_ticker = 0;
  if (0 > pthread_create(&(pkm->watchdog_thread), NULL,
                         pkw_run_watchdog, (void *) pkm)) {
    pk_log(PK_LOG_MANAGER_ERROR, "Failed to start watchdog thread.");
    return (pk_error = ERR_NO_THREAD);
  }
  return 0;
}

void pkw_stop_watchdog(struct pk_manager *pkm)
{
  pk_global_watchdog_ticker = -1;
  pthread_join(pkm->watchdog_thread, NULL);
}
