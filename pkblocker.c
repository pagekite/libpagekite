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

#include "includes.h"
#include <sys/time.h>

#include "utils.h"
#include "pkerror.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"


int pkb_add_job(struct pk_job_pile* pkj, pk_job_t job, void* data)
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

int pkb_get_job(struct pk_job_pile* pkj, struct pk_job* dest)
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

void pkb_clear_transient_flags(struct pk_manager* pkm)
{
  int i;
  struct pk_frontend* fe;
  for (i = 0, fe = pkm->frontends; i < pkm->frontend_max; i++, fe++) {
    fe->conn.status &= ~FE_STATUS_REJECTED;
    fe->conn.status &= ~FE_STATUS_LAME;
    fe->conn.status &= ~FE_STATUS_IS_FAST;
    fe->conn.status &= ~FE_STATUS_IN_DNS;
  }
}

void pkb_choose_frontends(struct pk_manager* pkm)
{
  int i, wanted;
  struct pk_frontend* fe;
  struct pk_frontend* highpri = NULL;

  for (i = 0, fe = pkm->frontends; i < pkm->frontend_max; i++, fe++) {
    if ((fe->ai) &&
        (fe->priority) &&
        ((highpri == NULL) || (highpri->priority > fe->priority)) &&
        (!(fe->conn.status & (FE_STATUS_REJECTED|FE_STATUS_LAME))))
      highpri = fe;
  }
  if (highpri != NULL)
    highpri->conn.status |= FE_STATUS_IS_FAST;

  wanted = 0;
  for (i = 0, fe = pkm->frontends; i < pkm->frontend_max; i++, fe++) {
    /* If it's nailed up or fast: we want it. */
    if ((fe->conn.status & FE_STATUS_NAILED_UP) ||
        (fe->conn.status & FE_STATUS_IS_FAST)) {
      fe->conn.status |= FE_STATUS_WANTED;
    }
    /* Otherwise, we don't! */
    else
      fe->conn.status &= ~FE_STATUS_WANTED;

    /* Rejecting us or going lame overrides other concerns. */
    if ((fe->conn.status & FE_STATUS_REJECTED) ||
        (fe->conn.status & FE_STATUS_LAME)) {
      fe->conn.status &= ~FE_STATUS_WANTED;
    }

    /* Count how many we're aiming for. */
    if (fe->conn.status & (FE_STATUS_WANTED|FE_STATUS_IN_DNS)) wanted++;
  }

  if (wanted == 0) {
    /* None?  Uh oh, best accept anything non-broken at this point... */
    for (i = 0, fe = pkm->frontends; i < pkm->frontend_max; i++, fe++) {
      if ((fe->ai != NULL) &&
          !(fe->conn.status & (FE_STATUS_REJECTED|FE_STATUS_LAME))) {
        pk_log(PK_LOG_MANAGER_DEBUG,
               "None wanted, going for random %s", fe->fe_hostname);
        fe->conn.status |= FE_STATUS_WANTED;
        break;
      }
    }
  }
}

void pkb_check_kites_dns(struct pk_manager* pkm)
{
  int i, j, rv;
  struct pk_frontend* fe;
  struct pk_pagekite* kite;
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  char buffer[128];

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  for (i = 0, kite = pkm->kites; i < pkm->kite_max; i++, kite++) {
    rv = getaddrinfo(kite->public_domain, NULL, &hints, &result);
    if (rv == 0) {
      for (rp = result; rp != NULL; rp = rp->ai_next) {
        for (j = 0, fe = pkm->frontends; j < pkm->frontend_max; j++, fe++) {
          if ((fe->ai) &&
              (0 == addrcmp(fe->ai->ai_addr, rp->ai_addr))) {
            pk_log(PK_LOG_MANAGER_DEBUG, "In DNS for %s: %s",
                                         kite->public_domain,
                                         in_addr_to_str(fe->ai->ai_addr,
                                                        buffer, 128));
            fe->conn.status |= FE_STATUS_IN_DNS;
          }
        }
      }
      freeaddrinfo(result);
    }
  }
}

void* pkb_frontend_ping(void* void_fe) {
  struct pk_frontend* fe = (struct pk_frontend*) void_fe;
  struct timeval tv1, tv2;
  char buffer[1024], printip[1024];
  int sockfd, bytes, want;

  fe->priority = 0;
  in_addr_to_str(fe->ai->ai_addr, printip, 1024);

  gettimeofday(&tv1, NULL);
  if ((0 > (sockfd = socket(fe->ai->ai_family, fe->ai->ai_socktype,
                            fe->ai->ai_protocol))) ||
      (0 > connect(sockfd, fe->ai->ai_addr, fe->ai->ai_addrlen)) ||
      (0 > write(sockfd, PK_FRONTEND_PING, strlen(PK_FRONTEND_PING))))
  {
    if (sockfd >= 0)
      close(sockfd);
    pk_log(PK_LOG_MANAGER_DEBUG, "Ping %s failed! (connect)", printip);
    sleep(2); /* We don't want to return first! */
    return NULL;
  }
  want = strlen(PK_FRONTEND_PONG);
  bytes = timed_read(sockfd, buffer, want, 1000);
  if ((bytes != want) ||
      (0 != strncmp(buffer, PK_FRONTEND_PONG, want))) {
    pk_log(PK_LOG_MANAGER_DEBUG, "Ping %s failed! (read=%d)", printip, bytes);
    sleep(2); /* We don't want to return first! */
    return NULL;
  }
  close(sockfd);
  gettimeofday(&tv2, NULL);

  fe->priority = (tv2.tv_sec - tv1.tv_sec) * 1000
               + (tv2.tv_usec - tv1.tv_usec) / 1000;
  pk_log(PK_LOG_MANAGER_DEBUG, "Ping %s: %dms", printip, fe->priority);
  return NULL;
}

void pkb_check_frontend_pingtimes(struct pk_manager* pkm)
{
  int j;
  struct pk_frontend* fe;
  pthread_t pt = 0;
  for (j = 0, fe = pkm->frontends; j < pkm->frontend_max; j++, fe++) {
    if (fe->ai)
      pthread_create(&pt, NULL, pkb_frontend_ping, (void *) fe);
  }
  if (pt) {
    /* We only wait for one at random - usually we only care about the
     * fastest anyway.  The others will return in their own good time.
     */
    pthread_join(pt, NULL);
  }
}

void pkb_update_dns(struct pk_manager* pkm)
{
  int j, len, missing, rlen;
  struct pk_frontend* fe;
  struct pk_pagekite* kite;
  char printip[128], get_result[10240];
  char address_list[1024], payload[2048], signature[2048], url[2048], *alp;

  address_list[0] = '\0';
  alp = address_list;

  missing = 0;
  for (j = 0, fe = pkm->frontends; j < pkm->frontend_max; j++, fe++) {
    if ((fe->ai) &&
        (fe->conn.sockfd >= 0) &&
        (fe->conn.status & FE_STATUS_WANTED)) {
      if (NULL != in_addr_to_str(fe->ai->ai_addr, printip, 128)) {
        len = strlen(printip);
        if (len < 1000-(alp-address_list)) {
          if (alp != address_list) *alp++ = ',';
          strcpy(alp, printip);
          alp += len;
        }
      }
      if (!(fe->conn.status & FE_STATUS_IN_DNS)) missing++;
    }
  }
  if (!missing) return;

  for (j = 0, kite = pkm->kites; j < pkm->kite_max; kite++, j++) {
    if (kite->protocol != NULL) {
      sprintf(payload, "%s:%s", kite->public_domain, address_list);
      pk_sign(NULL, kite->auth_secret, payload, 100, signature);

      sprintf(url, pkm->dynamic_dns_url,
              kite->public_domain, address_list, signature);
      rlen = http_get(url, get_result, 10240);

      pk_log(PK_LOG_MANAGER_DEBUG, "*** DDNS UP: %s", url);
      pk_log(PK_LOG_MANAGER_DEBUG, "*** RESULT(%d): %s", rlen, get_result);
    }
  }
}

void pkb_log_fe_status(struct pk_manager* pkm)
{
  int j;
  struct pk_frontend* fe;
  char printip[128];
  for (j = 0, fe = pkm->frontends; j < pkm->frontend_max; j++, fe++) {
    if (fe->ai) {
      if (NULL != in_addr_to_str(fe->ai->ai_addr, printip, 128)) {
        pk_log(PK_LOG_MANAGER_DEBUG, "  0x%8.8x %s", fe->conn.status, printip);
      }
    }
  }
}

void pkb_check_world(struct pk_manager* pkm)
{
  pk_log(PK_LOG_MANAGER_DEBUG, "Checking state of world...");
  pkb_clear_transient_flags(pkm);
  pkb_check_frontend_pingtimes(pkm);
  pkb_check_kites_dns(pkm);
  pkb_log_fe_status(pkm);
  pkm->last_world_update = time(0);
}

void pkb_check_frontends(struct pk_manager* pkm)
{
  pk_log(PK_LOG_MANAGER_DEBUG, "Checking frontends...");
  pkb_choose_frontends(pkm);
  pkb_log_fe_status(pkm);
  pkm_reconnect_all(pkm);
  /* FIXME: Disconnect from idle front-ends we don't care about anymore. */
  if (pkm->dynamic_dns_url) pkb_update_dns(pkm);
}

void* pkb_run_blocker(void *void_pkm)
{
  struct pk_job job;
  struct pk_manager* pkm = (struct pk_manager*) void_pkm;
  pk_log(PK_LOG_MANAGER_DEBUG, "Started blocking thread.");

  while (1) {
    pkb_get_job(&(pkm->blocking_jobs), &job);
    switch (job.job) {
      case PK_NO_JOB:
        break;
      case PK_CHECK_WORLD:
        pkb_check_world((struct pk_manager*) job.data);
        /* Fall through to PK_CHECK_FRONTENDS */
      case PK_CHECK_FRONTENDS:
        pkb_check_frontends((struct pk_manager*) job.data);
        break;
      case PK_QUIT:
        pk_log(PK_LOG_MANAGER_DEBUG, "Exiting blocking thread.");
        return NULL;
    }
  }
}


int pkb_start_blocker(struct pk_manager *pkm)
{
  if (0 > pthread_create(&(pkm->blocking_thread), NULL,
                         pkb_run_blocker, (void *) pkm)) {
    pk_log(PK_LOG_MANAGER_ERROR, "Failed to start blocking thread.");
    return (pk_error = ERR_NO_THREAD);
  }
  return 0;
}

void pkb_stop_blocker(struct pk_manager *pkm)
{
  pkb_add_job(&(pkm->blocking_jobs), PK_QUIT, NULL);
  pthread_join(pkm->blocking_thread, NULL);
}
