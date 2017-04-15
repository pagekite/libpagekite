/******************************************************************************
pkblocker.c - Blocking tasks handled outside the main event loop.

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
#include "pkerror.h"
#include "pkstate.h"
#include "pkhooks.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"
#include "pklua.h"
#if HAVE_RELAY
#include "pkrelay.h"
#endif


int pkb_add_job(struct pk_job_pile* pkj, pk_job_t job,
                int int_data, void* ptr_data)
{
  int i;
  PK_TRACE_FUNCTION;

  pthread_mutex_lock(&(pkj->mutex));
  for (i = 0; i < pkj->max; i++) {
    if ((pkj->pile+i)->job == PK_NO_JOB) {
      PK_ADD_MEMORY_CANARY(pkj->pile+i);
      (pkj->pile+i)->job = job;
      (pkj->pile+i)->int_data = int_data;
      (pkj->pile+i)->ptr_data = ptr_data;
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
      dest->int_data = (pkj->pile+i)->int_data;
      dest->ptr_data = (pkj->pile+i)->ptr_data;
      (pkj->pile+i)->job = PK_NO_JOB;
      (pkj->pile+i)->int_data = 0;
      (pkj->pile+i)->ptr_data = NULL;
      pkj->count -= 1;
      pthread_mutex_unlock(&(pkj->mutex));
      return 1;
    }
  }

  dest->job = PK_NO_JOB;
  dest->int_data = 0;
  dest->ptr_data = NULL;
  pthread_mutex_unlock(&(pkj->mutex));
  PK_CHECK_MEMORY_CANARIES;
  return -1;
}

void pkb_clear_transient_flags(struct pk_manager* pkm)
{
  int i;
  struct pk_tunnel* fe;

  PK_TRACE_FUNCTION;

  for (i = 0, fe = pkm->tunnels; i < pkm->tunnel_max; i++, fe++) {
    fe->conn.status &= ~FE_STATUS_REJECTED;
    fe->conn.status &= ~FE_STATUS_LAME;
    fe->conn.status &= ~FE_STATUS_IS_FAST;
  }
}

void pkb_choose_tunnels(struct pk_manager* pkm)
{
  int i, wanted, wantn, highpri, prio;
  struct pk_tunnel* fe;
  struct pk_tunnel* highpri_fe;

  PK_TRACE_FUNCTION;

  /* Clear WANTED flag... */
  for (i = 0, fe = pkm->tunnels; i < pkm->tunnel_max; i++, fe++) {
    if (fe->ai.ai_addr && fe->fe_hostname) {
      fe->conn.status &= ~(FE_STATUS_WANTED|FE_STATUS_IS_FAST);
    }
  }

  /* Choose N fastest: this is inefficient, but trivially correct. */
  for (wantn = 0; wantn < pkm->want_spare_frontends+1; wantn++) {
    highpri_fe = NULL;
    highpri = 1024000;
    for (i = 0, fe = pkm->tunnels; i < pkm->tunnel_max; i++, fe++) {
      /* Is tunnel really a front-end? */
      if ((fe->fe_hostname == NULL) || (fe->ai.ai_addr == NULL)) continue;

      /* Is tunnel in the middle of connecting still? That's a bad sign,
       * don't consider it a candidate for "fastest" in this round. */
      if (fe->conn.status & CONN_STATUS_CHANGING) continue;

      prio = fe->priority + (25 * fe->error_count);
      if ((fe->ai.ai_addr) &&
          (fe->fe_hostname) &&
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
  for (i = 0, fe = pkm->tunnels; i < pkm->tunnel_max; i++, fe++) {
    /* Is tunnel really a front-end? */
    if ((fe->fe_hostname == NULL) || (fe->ai.ai_addr == NULL)) continue;

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
  for (i = 0, fe = pkm->tunnels; i < pkm->tunnel_max; i++, fe++) {
    if ((fe->ai.ai_addr != NULL) &&
        (fe->fe_hostname != NULL) &&
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
  for (i = 0, fe = pkm->tunnels; i < pkm->tunnel_max; i++, fe++) {
    if ((fe->ai.ai_addr != NULL) &&
        (fe->fe_hostname != NULL) &&
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

int pkb_check_kites_dns(struct pk_manager* pkm)
{
  int i, j, rv;
  int in_dns = 0;
  int recently_in_dns = 0;
  time_t ddns_window;
  struct pk_tunnel* fe;
  struct pk_tunnel* dns_fe;
  struct pk_pagekite* kite;
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  char buffer[128];
  int cleared_flags = 0;

  PK_TRACE_FUNCTION;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  /* Walk through kite list, look each up in DNS and update the
   * tunnel flags as appropriate.
   */
  for (i = 0, kite = pkm->kites; i < pkm->kite_max; i++, kite++) {
    rv = getaddrinfo(kite->public_domain, NULL, &hints, &result);
    if ((rv == 0) && (result != NULL)) {
      if (!cleared_flags) {
        /* Clear DNS flag everywhere, once we know DNS is responding. */
        for (j = 0, fe = pkm->tunnels; j < pkm->tunnel_max; j++, fe++) {
          fe->conn.status &= ~FE_STATUS_IN_DNS;
        }
        cleared_flags = 1;
      }
      for (rp = result; rp != NULL; rp = rp->ai_next) {
        for (j = 0, fe = pkm->tunnels; j < pkm->tunnel_max; j++, fe++) {
          if (fe->ai.ai_addr && fe->fe_hostname) {
            if (0 == addrcmp(fe->ai.ai_addr, rp->ai_addr)) {
              pk_log(PK_LOG_MANAGER_DEBUG, "In DNS for %s: %s",
                                           kite->public_domain,
                                           in_ipaddr_to_str(fe->ai.ai_addr,
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

  /*
   * If flags weren't cleared, then the network is probably down: bail out!
   */
  if (!cleared_flags) return 1;

  /* FIXME: We should really get this from the TTL of the DNS record itself,
   *        not from a hard coded magic number.
   */
  ddns_window = time(0) - PK_DDNS_UPDATE_INTERVAL_MIN;

  /* Walk through the list of tunnels and rewnew the FE_STATUS_IN_DNS
   * if they were either last updated within our window.
   */
  dns_fe = NULL;
  for (j = 0, fe = pkm->tunnels; j < pkm->tunnel_max; j++, fe++) {
    if (fe->ai.ai_addr && fe->fe_hostname) {
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
   * we are using many tunnels at once, but still better than nothing.
   */
  if (in_dns < 1 && dns_fe) {
    dns_fe->conn.status |= FE_STATUS_IN_DNS;
  }

  PK_CHECK_MEMORY_CANARIES;
  return 0;
}

int pkb_check_frontend_dns(struct pk_manager* pkm)
{
  int i, changes, have_nulls;
  time_t obsolete;
  struct pk_tunnel* fe;
  char* last_fe_hostname;

  PK_TRACE_FUNCTION;

  /* Walk through frontend list, look each up in DNS and add new entries
   * as necessary.
   */
  changes = 0;
  have_nulls = 0;
  last_fe_hostname = "";
  for (i = 0, fe = pkm->tunnels; i < pkm->tunnel_max; i++, fe++) {
    if ((fe->fe_hostname != NULL) &&
        (0 != strcmp(fe->fe_hostname, last_fe_hostname)))
    {
      pk_log(PK_LOG_MANAGER_DEBUG, "Checking for new IPs: %s", fe->fe_hostname);
      changes += pkm_add_frontend(pkm, fe->fe_hostname, fe->fe_port, 0);
      last_fe_hostname = fe->fe_hostname;
    }
    if ((fe->fe_hostname != NULL) && (fe->ai.ai_addr == NULL)) {
      have_nulls += 1;
    }
  }
  pk_log(PK_LOG_MANAGER_DEBUG, "Found %d new IPs", changes);

  /* 2nd pass: walk through list and remove obsolete entries
   * We only do this if we have null records which will let us re-discover
   * everything again if DNS went away temporarily.
   */
  if (have_nulls)
  {
    obsolete = time(0) - (pkm->check_world_interval * 4);
    for (i = 0, fe = pkm->tunnels; i < pkm->tunnel_max; i++, fe++) {
      if ((fe->fe_hostname != NULL) &&
          (fe->ai.ai_addr != NULL) &&
          (fe->last_configured < obsolete) &&
          (fe->last_ping < obsolete) &&
          (fe->conn.sockfd <= 0))
      {
        free(fe->fe_hostname);
        free_addrinfo_data(&fe->ai);
        fe->fe_hostname = NULL;
      }
    }
  }

  PK_CHECK_MEMORY_CANARIES;
  return changes;
}


void* pkb_tunnel_ping(void* void_fe) {
  struct pk_tunnel* fe = (struct pk_tunnel*) void_fe;
  struct timeval tv1, tv2, to;
  char buffer[1024], printip[1024];
  int sockfd, bytes, want;

  PK_TRACE_FUNCTION;

  fe->priority = 0;
  in_addr_to_str(fe->ai.ai_addr, printip, 1024);

  if (pk_state.fake_ping) {
    fe->priority = 1 + (rand() % 500);
  }
  else {
    gettimeofday(&tv1, NULL);
    to.tv_sec = pk_state.socket_timeout_s;
    to.tv_usec = 0;
    if ((0 > (sockfd = PKS_socket(fe->ai.ai_family, fe->ai.ai_socktype,
                                  fe->ai.ai_protocol))) ||
        PKS_fail(PKS_setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &to, sizeof(to))) ||
        PKS_fail(PKS_setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *) &to, sizeof(to))) ||
        PKS_fail(PKS_connect(sockfd, fe->ai.ai_addr, fe->ai.ai_addrlen)) ||
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

    /* Magic number: 116 bytes is the shortest response we expect. It is
     * still long enough to contain the X-PageKite-Overloaded marker, if
     * present at all. */
    bytes = timed_read(sockfd, buffer, 116, 1000);
    buffer[116] = '\0';

    PKS_close(sockfd);

    want = strlen(PK_FRONTEND_PONG);
    if ((bytes < want) ||
        (0 != strncmp(buffer, PK_FRONTEND_PONG, want))) {
      if (fe->error_count < 999)
        fe->error_count += 1;
      pk_log(PK_LOG_MANAGER_DEBUG, "Ping %s failed! (read=%d)", printip, bytes);
      sleep(2); /* We don't want to return first! */
      return NULL;
    }
    gettimeofday(&tv2, NULL);

    fe->priority = ((tv2.tv_sec - tv1.tv_sec) * 1000)
                 + ((tv2.tv_usec - tv1.tv_usec) / 1000)
                 + 1;

    if (strcasestr(buffer, PK_FRONTEND_OVERLOADED) != NULL) {
      fe->priority += 1000;
      sleep(2); /* We don't want to return first! */
    }
  }

  if (fe->conn.status & (FE_STATUS_WANTED|FE_STATUS_IS_FAST))
  {
    /* Bias ping time to make old decisions a bit more sticky. We ignore
     * DNS though, to allow a bit of churn to spread the load around and
     * make sure new tunnels don't stay ignored forever. */
    fe->priority /= 10;
    fe->priority *= 9;
    if (fe->priority < 1) fe->priority = 1;
    pk_log(PK_LOG_MANAGER_DEBUG,
           "Ping %s: %dms (biased)", printip, fe->priority);
  }
  else {
    /* Add artificial +/-5% jitter to ping results */
    fe->priority *= ((rand() % 11) + 95);
    fe->priority /= 100;
    if (fe->priority < 1) fe->priority = 1;
    pk_log(PK_LOG_MANAGER_DEBUG, "Ping %s: %dms", printip, fe->priority);
  }

  PK_CHECK_MEMORY_CANARIES;
  return NULL;
}

void pkb_check_tunnel_pingtimes(struct pk_manager* pkm)
{
  int j;
  struct pk_tunnel* fe;

  PK_TRACE_FUNCTION;

  int first = 0;
  pthread_t first_pt;
  pthread_t pt;
  for (j = 0, fe = pkm->tunnels; j < pkm->tunnel_max; j++, fe++) {
    if (fe->ai.ai_addr && fe->fe_hostname) {
      if (0 == pthread_create(&pt, NULL, pkb_tunnel_ping, (void *) fe)) {
        if (first)
          pthread_detach(pt);
        else {
          first_pt = pt;
          first = 1;
        }
      }
    }
  }
  if (first) {
    /* Sleep, but only wait for the first one - usually we only care about the
     * fastest anyway.  The others will return in their own good time.
     */
    sleep(1);
    pthread_join(first_pt, NULL);
  }
}

int pkb_update_dns(struct pk_manager* pkm)
{
  int j, len, bogus, rlen;
  struct pk_tunnel* fe_list[1024]; /* Magic, bounded by address_list[] below */
  struct pk_tunnel** fes;
  struct pk_tunnel* fe;
  struct pk_pagekite* kite;
  char printip[128], get_result[10240], *result, *lastup;
  char address_list[1024], payload[2048], signature[2048], url[2048], *alp;

  PK_TRACE_FUNCTION;

  if (time(0) < pkm->last_dns_update + PK_DDNS_UPDATE_INTERVAL_MIN)
    return 0;

  address_list[0] = '\0';
  alp = address_list;

  fes = fe_list;
  *fes = NULL;

  bogus = 0;
  for (j = 0, fe = pkm->tunnels; j < pkm->tunnel_max; j++, fe++) {
    if (fe->ai.ai_addr && fe->fe_hostname && (fe->conn.sockfd >= 0)) {
      if (fe->conn.status & FE_STATUS_WANTED) {
        if (NULL != in_ipaddr_to_str(fe->ai.ai_addr, printip, 128)) {
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
  PK_CHECK_MEMORY_CANARIES;
  if (!bogus) return 0;
  if (!address_list[0]) return 0;

  bogus = 0;
  lastup = "";
  for (j = 0, kite = pkm->kites; j < pkm->kite_max; kite++, j++) {
    if ((kite->protocol[0] != '\0') &&
        (0 != strcasecmp(lastup, kite->public_domain))) {
      PKS_STATE(pkm->status = PK_STATUS_UPDATING_DNS);
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
        lastup = kite->public_domain;
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
  PK_CHECK_MEMORY_CANARIES;
  return bogus;
}

void pkb_log_fe_status(struct pk_manager* pkm)
{
  int j, ddnsup_ago;
  struct pk_tunnel* fe;
  char printip[128];
  char ddnsinfo[128];

  PK_TRACE_FUNCTION;

  for (j = 0, fe = pkm->tunnels; j < pkm->tunnel_max; j++, fe++) {
    if (fe->ai.ai_addr && fe->fe_hostname) {
      if (NULL != in_addr_to_str(fe->ai.ai_addr, printip, 128)) {
        ddnsinfo[0] = '\0';
        if (fe->last_ddnsup) {
          ddnsup_ago = time(0) - fe->last_ddnsup;
          sprintf(ddnsinfo, " (in DNS %us ago)", ddnsup_ago);
        }
        pk_log(PK_LOG_MANAGER_DEBUG,
               "Relay; status=0x%8.8x; errors=%d; info=%s%s%s%s%s%s%s%s%s",
               fe->conn.status,
               fe->error_count,
               printip,
               (fe->conn.status & CONN_STATUS_CHANGING) ? " changing": "",
               (fe->conn.status & FE_STATUS_REJECTED) ? " rejected": "",
               (fe->conn.status & FE_STATUS_WANTED) ? " wanted": "",
               (fe->conn.status & FE_STATUS_LAME) ? " lame": "",
               (fe->conn.status & FE_STATUS_IN_DNS) ? " in-DNS": "",
               (fe->conn.status & FE_STATUS_IS_FAST) ? " fast": "",
               (fe->conn.sockfd > 0) ? " live" : "",
               ddnsinfo);
      }
    }
  }
}

void pkb_check_world(struct pk_manager* pkm)
{
  PK_TRACE_FUNCTION;

  if (pkm->status == PK_STATUS_NO_NETWORK) {
    pk_log(PK_LOG_MANAGER_DEBUG, "Waiting for network... (v%s)", PK_VERSION);
    return;
  }
  pk_log(PK_LOG_MANAGER_DEBUG,
         "Checking state of world... (v%s)", PK_VERSION);
  better_srand(PK_RANDOM_DEFAULT);
  pkb_clear_transient_flags(pkm);
  pkb_check_tunnel_pingtimes(pkm);
  pkb_check_kites_dns(pkm);
  pkb_log_fe_status(pkm);
  pkm->last_world_update = time(0) + pkm->interval_fudge_factor;
  PK_CHECK_MEMORY_CANARIES;
}

void pkb_update_state(struct pk_manager* pkm, int dns_is_down, int problems)
{
  /* An update has happened, clear this flag. */
  pk_state.force_update = 0;
  if (problems == 0 && pk_state.live_tunnels > 0) {
    PKS_STATE(pkm->status = PK_STATUS_FLYING);
  }
  else if (pkm->status != PK_STATUS_REJECTED) {
    if (dns_is_down)
      pk_log(PK_LOG_MANAGER_INFO, "Network appears to be down.");
    PKS_STATE(pkm->status = (dns_is_down ? PK_STATUS_NO_NETWORK
                                         : PK_STATUS_PROBLEMS));
  }
}

void pkb_check_tunnels(struct pk_manager* pkm)
{
  int problems = 0;
  int dns_is_down = 0;
  PK_TRACE_FUNCTION;

  pk_log(PK_LOG_MANAGER_DEBUG,
         "Checking network & tunnels... (v%s)", PK_VERSION);

  dns_is_down = (0 != pkb_check_kites_dns(pkm));

  if (!dns_is_down && (pkb_check_frontend_dns(pkm) > 0)) {
    pkb_update_state(pkm, dns_is_down, problems);
    pkb_check_world(pkm);
  }

  pkb_choose_tunnels(pkm);
  pkb_log_fe_status(pkm);
  problems += pkm_reconnect_all(pkm, dns_is_down);

  if (!problems) pkm_disconnect_unused(pkm);

  if (pkm->dynamic_dns_url && (pkm->status != PK_STATUS_REJECTED)) {
    problems += pkb_update_dns(pkm);
  }

  pkb_update_state(pkm, dns_is_down, problems);
}

void* pkb_run_blocker(void *void_pkblocker)
{
  static time_t last_check_world = 0;
  static time_t last_check_tunnels = 0;
  struct pk_job job;
  struct pk_blocker* this = (struct pk_blocker*) void_pkblocker;
  struct pk_manager* pkm = this->manager;

#if HAVE_LUA
  /* Initialize Lua state for this thread/blocker. */
  if (pkm->lua != NULL) {
    this->lua = pklua_unlock_lua(pklua_get_locked_lua(pkm));
  }
#endif

  pk_log(PK_LOG_MANAGER_DEBUG, "Started blocking thread.");
  PK_HOOK(PK_HOOK_START_BLOCKER, 0, this, pkm);

  while (1) {
    pkb_get_job(&(pkm->blocking_jobs), &job);

    time_t now = time(0);
    switch (job.job) {
      case PK_NO_JOB:
        break;
      case PK_CHECK_WORLD:
        if ((now >= last_check_world + pkm->housekeeping_interval_min) &&
            (0 == pkm_reconfig_start((struct pk_manager*) job.ptr_data)))
        {
          if (PK_HOOK(PK_HOOK_CHECK_WORLD, 0, this, pkm)) {
            last_check_tunnels = now;
            pkb_check_world((struct pk_manager*) job.ptr_data);
            pkb_check_tunnels((struct pk_manager*) job.ptr_data);
            last_check_world = last_check_tunnels = time(0);
            PK_HOOK(PK_HOOK_CHECK_WORLD, 1, this, pkm);
          }
          pkm_reconfig_stop((struct pk_manager*) job.ptr_data);
        }
        break;
      case PK_CHECK_FRONTENDS:
        if ((now >= last_check_tunnels + pkm->housekeeping_interval_min) &&
            (0 == pkm_reconfig_start((struct pk_manager*) job.ptr_data)))
        {
          if (PK_HOOK(PK_HOOK_CHECK_TUNNELS, 0, this, pkm)) {
            last_check_tunnels = now;
            pkb_check_tunnels((struct pk_manager*) job.ptr_data);
            last_check_tunnels = time(0);
            PK_HOOK(PK_HOOK_CHECK_TUNNELS, 1, this, pkm);
          }
          pkm_reconfig_stop((struct pk_manager*) job.ptr_data);
        }
        break;
      case PK_ACCEPT_LUA:
#if HAVE_LUA
        pklua_socket_server_accepted(this->lua, job.int_data, job.ptr_data);
#endif
        break;
      case PK_RELAY_INCOMING:
#if HAVE_RELAY
        pkr_relay_incoming(this->lua, job.int_data, job.ptr_data);
#endif
        break;
      case PK_QUIT:
        /* Put the job back in the queue, in case there are many workers */
        pkb_add_job(&(pkm->blocking_jobs), PK_QUIT, 0, NULL);
        pk_log(PK_LOG_MANAGER_DEBUG, "Exiting blocking thread.");
        return NULL;
    }
  }
}

int pkb_start_blockers(struct pk_manager *pkm, int n)
{
  int i;
  for (i = 0; i < MAX_BLOCKING_THREADS && n > 0; i++) {
    if (pkm->blocking_threads[i] == NULL) {
      pkm->blocking_threads[i] = malloc(sizeof(struct pk_blocker));
      pkm->blocking_threads[i]->manager = pkm;
#if HAVE_LUA
      pkm->blocking_threads[i]->lua = NULL;
#endif
      if (0 > pthread_create(&(pkm->blocking_threads[i]->thread), NULL,
                             pkb_run_blocker,
                             (void *) pkm->blocking_threads[i])) {
        pk_log(PK_LOG_MANAGER_ERROR, "Failed to start blocking thread.");
#if HAVE_LUA
        pklua_close_lua(pkm->blocking_threads[i]->lua);
#endif
        free(pkm->blocking_threads[i]);
        pkm->blocking_threads[i] = NULL;
        return (pk_error = ERR_NO_THREAD);
      }
      n--;
    }
    else {
      pk_log(PK_LOG_MANAGER_ERROR, "Blocking thread %d already started?", i);
    }
  }
  return 0;
}

void pkb_stop_blockers(struct pk_manager *pkm)
{
  int i;
  for (i = 0; i < pkm->blocking_jobs.max; i++) {
    pkb_add_job(&(pkm->blocking_jobs), PK_QUIT, 0, NULL);
  }
  for (i = 0; i < MAX_BLOCKING_THREADS; i++) {
    if (pkm->blocking_threads[i] != NULL) {
      pthread_join(pkm->blocking_threads[i]->thread, NULL);
#if HAVE_LUA
      pklua_close_lua(pkm->blocking_threads[i]->lua);
#endif
      free(pkm->blocking_threads[i]);
      pkm->blocking_threads[i] = NULL;
    }
  }
}
