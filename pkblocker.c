/******************************************************************************
pkblocker.c - Blocking tasks handled outside the main event loop.

This file is Copyright 2011-2013, The Beanstalks Project ehf.

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

#include "common.h"
#include "utils.h"
#include "pkerror.h"
#include "pkstate.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"


int pkb_add_job(struct pk_job_pile* pkj, pk_job_t job, void* data)
{
  int i;
  PK_TRACE_FUNCTION;

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
  PK_TRACE_FUNCTION;

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

  PK_TRACE_FUNCTION;

  for (i = 0, fe = pkm->frontends; i < pkm->frontend_max; i++, fe++) {
    fe->conn.status &= ~FE_STATUS_REJECTED;
    fe->conn.status &= ~FE_STATUS_LAME;
    fe->conn.status &= ~FE_STATUS_IS_FAST;
    fe->conn.status &= ~FE_STATUS_IN_DNS;
  }
}

void pkb_choose_frontends(struct pk_manager* pkm)
{
  int i, wanted, wantn, highpri, prio;
  struct pk_frontend* fe;
  struct pk_frontend* highpri_fe;

  PK_TRACE_FUNCTION;

  /* Clear WANTED flag... */
  for (i = 0, fe = pkm->frontends; i < pkm->frontend_max; i++, fe++) {
    if (fe->ai) fe->conn.status &= ~(FE_STATUS_WANTED|FE_STATUS_IS_FAST);
  }

  /* Choose N fastest: this is inefficient, but trivially correct. */
  for (wantn = 0; wantn < pkm->want_spare_frontends+1; wantn++) {
    highpri_fe = NULL;
    highpri = 1024000;
    for (i = 0, fe = pkm->frontends; i < pkm->frontend_max; i++, fe++) {
      prio = fe->priority + (25 * fe->error_count);
      if ((fe->ai) &&
          (fe->priority) &&
          ((highpri_fe == NULL) || (highpri > prio)) &&
          (!(fe->conn.status & (FE_STATUS_IS_FAST
                               |FE_STATUS_REJECTED
                               |FE_STATUS_LAME)))) {
        highpri_fe = fe;
        highpri = prio;
      }
    }
    if (highpri_fe != NULL)
      highpri_fe->conn.status |= FE_STATUS_IS_FAST;
  }

  wanted = 0;
  for (i = 0, fe = pkm->frontends; i < pkm->frontend_max; i++, fe++) {
    /* If it's nailed up or fast: we want it. */
    if ((fe->conn.status & FE_STATUS_NAILED_UP) ||
        (fe->conn.status & FE_STATUS_IS_FAST)) {
      fe->conn.status |= FE_STATUS_WANTED;
      pk_log(PK_LOG_MANAGER_DEBUG,
             "Fast or nailed up, should use %s (status=%x)",
             fe->fe_hostname, fe->conn.status);
    }
    /* Otherwise, we don't! */
    else {
      fe->conn.status &= ~FE_STATUS_WANTED;
      if (fe->conn.status & FE_STATUS_IN_DNS) {
        pk_log(PK_LOG_MANAGER_DEBUG,
               "Not wanted, but in DNS (fallback): %s (status=%x)",
               fe->fe_hostname, fe->conn.status);
      }
    }

    /* Rejecting us or going lame overrides other concerns. */
    if ((fe->conn.status & FE_STATUS_REJECTED) ||
        (fe->conn.status & FE_STATUS_LAME)) {
      fe->conn.status &= ~FE_STATUS_WANTED;
      pk_log(PK_LOG_MANAGER_DEBUG,
             "Lame or rejecting, avoiding %s (status=%x)",
             fe->fe_hostname, fe->conn.status);
    }

    /* Count how many we're aiming for. */
    if (fe->conn.status & (FE_STATUS_WANTED|FE_STATUS_IN_DNS)) wanted++;
  }
  if (wanted) return;

  /* None wanted?  Uh oh, best accept anything non-broken at this point... */
  for (i = 0, fe = pkm->frontends; i < pkm->frontend_max; i++, fe++) {
    if ((fe->ai != NULL) &&
        !(fe->conn.status & (FE_STATUS_REJECTED|FE_STATUS_LAME))) {
      fe->conn.status |= FE_STATUS_WANTED;
      wanted++;
      pk_log(PK_LOG_MANAGER_INFO,
             "No front-end wanted, randomly using %s (status=%x)",
             fe->fe_hostname, fe->conn.status);
      break;
    }
  }
  if (wanted) return;

  /* Still none? Crazy town. Maybe a good front-end has been marked as
   * being lame because of duplicates and we've somehow forgotten it is
   * in DNS? Let's at least not disconnect. */
  for (i = 0, fe = pkm->frontends; i < pkm->frontend_max; i++, fe++) {
    if ((fe->ai != NULL) &&
        (fe->conn.sockfd > 0)) {
      fe->conn.status |= FE_STATUS_WANTED;
      wanted++;
      pk_log(PK_LOG_MANAGER_INFO,
             "No front-end wanted, keeping %s (status=%x)",
             fe->fe_hostname, fe->conn.status);
    }
  }
  if (wanted) return;

  /* If we get this far, we're hopeless. Log as error. */
  pk_log(PK_LOG_MANAGER_ERROR, "No front-end wanted! We are lame.");
}

void pkb_check_kites_dns(struct pk_manager* pkm)
{
  int i, j, rv;
  int in_dns = 0;
  int recently_in_dns = 0;
  time_t ddns_window;
  struct pk_frontend* fe;
  struct pk_frontend* dns_fe;
  struct pk_pagekite* kite;
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  char buffer[128];

  PK_TRACE_FUNCTION;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  /* Clear DNS flag... */
  for (j = 0, fe = pkm->frontends; j < pkm->frontend_max; j++, fe++) {
    fe->conn.status &= ~FE_STATUS_IN_DNS;
  }

  /* Walk through kite list, look each up in DNS and update the
   * frontend flags as appropriate.
   */
  for (i = 0, kite = pkm->kites; i < pkm->kite_max; i++, kite++) {
    rv = getaddrinfo(kite->public_domain, NULL, &hints, &result);
    if (rv == 0) {
      for (rp = result; rp != NULL; rp = rp->ai_next) {
        for (j = 0, fe = pkm->frontends; j < pkm->frontend_max; j++, fe++) {
          if (fe->ai) {
            if (0 == addrcmp(fe->ai->ai_addr, rp->ai_addr)) {
              pk_log(PK_LOG_MANAGER_DEBUG, "In DNS for %s: %s",
                                           kite->public_domain,
                                           in_ipaddr_to_str(fe->ai->ai_addr,
                                                            buffer, 128));
              fe->conn.status |= FE_STATUS_IN_DNS;
              fe->last_ddnsup = time(0);
              in_dns++;
            }
          }
        }
      }
      freeaddrinfo(result);
    }
  }

  /* FIXME: We should really get this from the TTL of the DNS record itself,
   *        not from a hard coded magic number.
   */
  ddns_window = time(0) - PK_DDNS_UPDATE_INTERVAL_MIN;

  /* Walk through the list of frontends and rewnew the FE_STATUS_IN_DNS
   * if they were either last updated within our window.
   */
  dns_fe = NULL;
  for (j = 0, fe = pkm->frontends; j < pkm->frontend_max; j++, fe++) {
    if (fe->ai) {
      if (fe->last_ddnsup > ddns_window) {
        fe->conn.status |= FE_STATUS_IN_DNS;
        in_dns++;
      }
      /* Figure out which FE was most recently seen in DNS, for use below */
      if (fe->last_ddnsup > recently_in_dns) {
        recently_in_dns = fe->last_ddnsup;
        dns_fe = fe;
      }
    }
  }

  /* If nothing was found in DNS, but we know there was stuff in DNS
   * before, then DNS is probably broken for us and the data in DNS is
   * unchanged. Keep the most recent one active!  This is incomplete if
   * we are using many frontends at once, but still better than nothing.
   */
  if (in_dns < 1 && dns_fe) {
    dns_fe->conn.status |= FE_STATUS_IN_DNS;
  }
}

void* pkb_frontend_ping(void* void_fe) {
  struct pk_frontend* fe = (struct pk_frontend*) void_fe;
  struct timeval tv1, tv2;
  char buffer[1024], printip[1024];
  int sockfd, bytes, want;

  PK_TRACE_FUNCTION;

  fe->priority = 0;
  in_addr_to_str(fe->ai->ai_addr, printip, 1024);

  if (pk_state.fake_ping) {
    fe->priority = rand() % 500;
  }
  else {
    gettimeofday(&tv1, NULL);
    if ((0 > (sockfd = PKS_socket(fe->ai->ai_family, fe->ai->ai_socktype,
                                  fe->ai->ai_protocol))) ||
        PKS_fail(PKS_connect(sockfd, fe->ai->ai_addr, fe->ai->ai_addrlen)) ||
        PKS_fail(PKS_write(sockfd, PK_FRONTEND_PING, strlen(PK_FRONTEND_PING))))
    {
      if (sockfd >= 0){
        PKS_close(sockfd);
      }
      if (fe->error_count < 999)
        fe->error_count += 1;
      pk_log(PK_LOG_MANAGER_DEBUG, "Ping %s failed! (connect)", printip);
      sleep(2); /* We don't want to return first! */
      return NULL;
    }
    want = strlen(PK_FRONTEND_PONG);
    bytes = timed_read(sockfd, buffer, want, 1000);
    if ((bytes != want) ||
        (0 != strncmp(buffer, PK_FRONTEND_PONG, want))) {
      if (fe->error_count < 999)
        fe->error_count += 1;
      pk_log(PK_LOG_MANAGER_DEBUG, "Ping %s failed! (read=%d)", printip, bytes);
      sleep(2); /* We don't want to return first! */
      return NULL;
    }
    PKS_close(sockfd);
    gettimeofday(&tv2, NULL);

    fe->priority = (tv2.tv_sec - tv1.tv_sec) * 1000
                 + (tv2.tv_usec - tv1.tv_usec) / 1000;
  }

  if (fe->conn.status & (FE_STATUS_WANTED|FE_STATUS_IS_FAST))
  {
    /* Bias ping time to make old decisions a bit more sticky. We ignore
     * DNS though, to allow a bit of churn to spread the load around and
     * make sure new frontends don't stay ignored forever. */
    fe->priority /= 10;
    fe->priority *= 9;
    pk_log(PK_LOG_MANAGER_DEBUG,
           "Ping %s: %dms (biased)", printip, fe->priority);
  }
  else {
    /* Add artificial +/-5% jitter to ping results */
    fe->priority *= ((rand() % 11) + 95);
    fe->priority /= 100;
    pk_log(PK_LOG_MANAGER_DEBUG, "Ping %s: %dms", printip, fe->priority);
  }
  return NULL;
}

void pkb_check_frontend_pingtimes(struct pk_manager* pkm)
{
  int j;
  struct pk_frontend* fe;

  PK_TRACE_FUNCTION;

  pthread_t first = 0;
  pthread_t pt = 0;
  for (j = 0, fe = pkm->frontends; j < pkm->frontend_max; j++, fe++) {
    if (fe->ai) {
      if (0 == pthread_create(&pt, NULL, pkb_frontend_ping, (void *) fe)) {
        if (first)
          pthread_detach(pt);
        else
          first = pt;
      }
    }
  }
  if (first) {
    /* Sleep, but only wait for the first one - usually we only care about the
     * fastest anyway.  The others will return in their own good time.
     */
    sleep(1);
    pthread_join(first, NULL);
  }
}

int pkb_update_dns(struct pk_manager* pkm)
{
  int j, len, bogus, rlen;
  struct pk_frontend* fe_list[1024]; /* Magic, bounded by address_list[] below */
  struct pk_frontend** fes;
  struct pk_frontend* fe;
  struct pk_pagekite* kite;
  char printip[128], get_result[10240], *result;
  char address_list[1024], payload[2048], signature[2048], url[2048], *alp;

  PK_TRACE_FUNCTION;

  if (time(0) < pkm->last_dns_update + PK_DDNS_UPDATE_INTERVAL_MIN)
    return 0;

  address_list[0] = '\0';
  alp = address_list;

  fes = fe_list;
  *fes = NULL;

  bogus = 0;
  for (j = 0, fe = pkm->frontends; j < pkm->frontend_max; j++, fe++) {
    if ((fe->ai) && (fe->conn.sockfd >= 0)) {
      if (fe->conn.status & FE_STATUS_WANTED) {
        if (NULL != in_ipaddr_to_str(fe->ai->ai_addr, printip, 128)) {
          len = strlen(printip);
          if (len < 1000-(alp-address_list)) {
            if (alp != address_list) *alp++ = ',';
            strcpy(alp, printip);
            alp += len;
            *fes++ = fe;
            *fes = NULL;
          }
        }
        if (!(fe->conn.status & FE_STATUS_IN_DNS) || pk_state.force_update)
          bogus++;
      }
      else /* Stuff in DNS that shouldn't be also triggers updates */
        if (fe->conn.status & FE_STATUS_IN_DNS) bogus++;
    }
  }
  if (!bogus) return 0;
  if (!address_list[0]) return 0;

  bogus = 0;
  for (j = 0, kite = pkm->kites; j < pkm->kite_max; kite++, j++) {
    if (kite->protocol[0] != '\0') {
      PKS_STATE(pkm->status = PK_STATUS_DYNDNS);
      sprintf(payload, "%s:%s", kite->public_domain, address_list);
      pk_sign(NULL, kite->auth_secret, payload, 100, signature);

      sprintf(url, pkm->dynamic_dns_url,
              kite->public_domain, address_list, signature);
      rlen = http_get(url, get_result, 10240);

      if (rlen < 1) {
        pk_log(PK_LOG_MANAGER_ERROR, "DDNS: No response from %s", url);
        bogus++;
      }
      else {
        result = skip_http_header(rlen, get_result);
        if ((strncasecmp(result, "nochg", 5) == 0) ||
            (strncasecmp(result, "good", 4) == 0)) {
          pk_log(PK_LOG_MANAGER_INFO, "DDNS: Update OK, %s=%s",
                                      kite->public_domain, address_list);
          for (fes = fe_list; *fes; fes++) {
            (*fes)->last_ddnsup = time(0);
            (*fes)->conn.status |= FE_STATUS_IN_DNS;
          }
        }
        else {
          result[7] = '\0';
          pk_log(PK_LOG_MANAGER_ERROR, "DDNS: Update failed for %s (%s -> %s)",
                                       kite->public_domain, url, result);
          bogus++;
        }
      }
    }
  }

  pkm->last_dns_update = time(0);
  return bogus;
}

void pkb_log_fe_status(struct pk_manager* pkm)
{
  int j, ddnsup_ago;
  struct pk_frontend* fe;
  char printip[128];
  char ddnsinfo[128];

  PK_TRACE_FUNCTION;

  for (j = 0, fe = pkm->frontends; j < pkm->frontend_max; j++, fe++) {
    if (fe->ai) {
      if (NULL != in_addr_to_str(fe->ai->ai_addr, printip, 128)) {
        ddnsinfo[0] = '\0';
        if (fe->last_ddnsup) {
          ddnsup_ago = time(0) - fe->last_ddnsup;
          sprintf(ddnsinfo, " (in dns %us ago)", ddnsup_ago);
        }
        pk_log(PK_LOG_MANAGER_DEBUG, "0x%8.8x E:%d %s%s%s",
                                     fe->conn.status,
                                     fe->error_count,
                                     printip,
                                     (fe->conn.sockfd > 0) ? " live" : "",
                                     ddnsinfo);
      }
    }
  }
}

void pkb_check_world(struct pk_manager* pkm)
{
  PK_TRACE_FUNCTION;

  if (pkm->status == PK_STATUS_NO_NETWORK) return;
  pk_log(PK_LOG_MANAGER_DEBUG, "Checking state of world...");
  pkb_clear_transient_flags(pkm);
  pkb_check_kites_dns(pkm);
  pkb_check_frontend_pingtimes(pkm);
  pkb_log_fe_status(pkm);
  pkm->last_world_update = time(0) + pkm->interval_fudge_factor;
}

void pkb_check_frontends(struct pk_manager* pkm)
{
  int problems = 0;
  PK_TRACE_FUNCTION;

  if (pkm->status == PK_STATUS_NO_NETWORK) return;
  pk_log(PK_LOG_MANAGER_DEBUG, "Checking frontends...");

  pkb_check_kites_dns(pkm);
  pkb_choose_frontends(pkm);
  pkb_log_fe_status(pkm);

  problems += pkm_reconnect_all(pkm);

  if (!problems) pkm_disconnect_unused(pkm);

  if (pkm->dynamic_dns_url && (pkm->status != PK_STATUS_REJECTED)) {
    problems += pkb_update_dns(pkm);
  }

  /* An update has happened, clear this flag. */
  pk_state.force_update = 0;
  if (problems == 0 && pk_state.live_frontends > 0) {
    PKS_STATE(pkm->status = PK_STATUS_FLYING);
  }
  else if (pkm->status != PK_STATUS_REJECTED) {
    PKS_STATE(pkm->status = PK_STATUS_PROBLEMS);
  }
}

void* pkb_run_blocker(void *void_pkm)
{
  time_t last_check_world = 0;
  time_t last_check_frontends = 0;
  struct pk_job job;
  struct pk_manager* pkm = (struct pk_manager*) void_pkm;
  pk_log(PK_LOG_MANAGER_DEBUG, "Started blocking thread.");

  while (1) {
    pkb_get_job(&(pkm->blocking_jobs), &job);
    switch (job.job) {
      case PK_NO_JOB:
        break;
      case PK_CHECK_WORLD:
        if (time(0) >= last_check_world + pkm->housekeeping_interval_min) {
          pkb_check_world((struct pk_manager*) job.data);
          pkb_check_frontends((struct pk_manager*) job.data);
          last_check_world = last_check_frontends = time(0);
        }
        break;
      case PK_CHECK_FRONTENDS:
        if (time(0) >= last_check_frontends + pkm->housekeeping_interval_min) {
          pkb_check_frontends((struct pk_manager*) job.data);
          last_check_frontends = time(0);
        }
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
