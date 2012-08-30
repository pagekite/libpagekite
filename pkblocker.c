/******************************************************************************
pkblocker.c - Blocking tasks handled outside the main event loop.

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

Note: For alternate license terms, see the file COPYING.md.

******************************************************************************/

#define _GNU_SOURCE 1
#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <ev.h>

#include "utils.h"
#include "pkerror.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"


int pkm_add_job(struct pk_job_pile* pkj, pk_job_t job, void* data)
{
  int i;
  pthread_mutex_lock(&(pkj->mutex));

  for (i = 0; i < pkj->max; i++) {
    if ((pkj->pile+i)->job == PK_NO_JOB) {
      (pkj->pile+i)->job = job;
      (pkj->pile+i)->data = data;
      pkj->count += 1;
      pthread_cond_signal(&(pkj->cond));
      pthread_mutex_unlock(&(pkj->mutex));
      return 1;
    }
  }
  pthread_mutex_unlock(&(pkj->mutex));
  return -1;
}

int pkm_get_job(struct pk_job_pile* pkj, struct pk_job* dest)
{
  int i;
  pthread_mutex_lock(&(pkj->mutex));
  while (pkj->count == 0)
    pthread_cond_wait(&(pkj->cond), &(pkj->mutex));

  for (i = 0; i < pkj->max; i++) {
    if ((pkj->pile+i)->job != PK_NO_JOB) {
      dest->job = (pkj->pile+i)->job;
      dest->data = (pkj->pile+i)->data;
      (pkj->pile+i)->job = PK_NO_JOB;
      (pkj->pile+i)->data = NULL;
      pkj->count -= 1;
      pthread_mutex_unlock(&(pkj->mutex));
      return 1;
    }
  }

  dest->job = PK_NO_JOB;
  dest->data = NULL;
  pthread_mutex_unlock(&(pkj->mutex));
  return -1;
}


void pkm_check_world(struct pk_manager* pkm) {
  pk_log(PK_LOG_MANAGER_DEBUG, "Checking state of world...");

  /* FIXME: Look up in DNS which front-ends need to be up. */
  /* FIXME: Measure ping-times for all known down front-ends... */
  /* FIXME: Choose favorite front-end based on ping times and flags*/
}

void pkm_check_frontends(struct pk_manager* pkm) {
  pk_log(PK_LOG_MANAGER_DEBUG, "Checking frontends...");

  /* FIXME: Connect to all front-ends that are wanted but missing. */
  pkm_reconnect_all(pkm);

  /* FIXME: Disconnect from idle front-ends we don't care about anymore. */
  /* FIXME: Update DNS if anything changed.  */
}

void* pkm_run_blocker(void *void_pkm) {
  struct pk_job job;
  struct pk_manager* pkm = (struct pk_manager*) void_pkm;
  pk_log(PK_LOG_MANAGER_DEBUG, "Started blocking thread.");

  while (1) {
    pkm_get_job(&(pkm->blocking_jobs), &job);
    switch (job.job) {
      case PK_NO_JOB:
        break;
      case PK_CHECK_WORLD:
        pkm_check_world((struct pk_manager*) job.data);
        /* Fall through to PK_CHECK_FRONTENDS */
      case PK_CHECK_FRONTENDS:
        pkm_check_frontends((struct pk_manager*) job.data);
        break;
      case PK_QUIT:
        pk_log(PK_LOG_MANAGER_DEBUG, "Exiting blocking thread.");
        return NULL;
    }
  }
}


int pkm_start_blocker(struct pk_manager *pkm) {
  if (0 > pthread_create(&(pkm->blocking_thread), NULL,
                         pkm_run_blocker, (void *) pkm)) {
    pk_log(PK_LOG_MANAGER_ERROR, "Failed to start blocking thread.");
    return (pk_error = ERR_NO_THREAD);
  }
  pkm_add_job(&(pkm->blocking_jobs), PK_CHECK_FRONTENDS, pkm);
  return 0;
}

void pkm_stop_blocker(struct pk_manager *pkm) {
  pkm_add_job(&(pkm->blocking_jobs), PK_QUIT, NULL);
  pthread_join(pkm->blocking_thread, NULL);
}


