/******************************************************************************
pagekite.c - High level library interface

*******************************************************************************

This file is Copyright 2012-2020, The Beanstalks Project ehf.

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

#include "pagekite.h"

#include "pkcommon.h"
#include "pkutils.h"
#include "pkstate.h"
#include "pkerror.h"
#include "pkhooks.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"
#if HAVE_RELAY
#include "pkrelay.h"
#endif

#define PK_DEFAULT_FLAGS (PK_WITH_SSL | PK_WITH_IPV4 | PK_WITH_IPV6 | \
                          PK_WITH_DYNAMIC_FE_LIST | PK_WITH_SRAND_RESEED)


static struct pk_manager* PK_MANAGER(pagekite_mgr pkm) {
  return (struct pk_manager*) pkm;
}


pagekite_mgr pagekite_init(
  const char* app_id,
  int max_kites,
  int max_frontends,
  int max_conns,
  const char* dyndns_url,
  int flags,
  int verbosity)
{
  SSL_CTX* ssl_ctx;
  struct pk_manager *pkm_;

  if (flags & PK_WITH_DEFAULTS || !flags) flags |= PK_DEFAULT_FLAGS;

  if (flags & PK_WITH_SRAND_RESEED) better_srand(PK_RANDOM_RESEED_SRAND);

#ifdef _MSC_VER
  WSADATA wsa_data;
  int r;
  if (0 != (r = WSAStartup(MAKEWORD(2, 2), &wsa_data))) {
    pk_log(PK_LOG_ERROR, "libpagekite: Error during WSAStartup: %d\n", r);
    return pk_err_null(ERR_WSA_STARTUP);
  }
#endif

  pks_global_init(PK_LOG_NORMAL);
  if (verbosity < 0x100) {
    pk_state.log_mask = ((verbosity < 0) ? PK_LOG_ERRORS :
                        ((verbosity < 1) ? PK_LOG_NORMAL :
                        ((verbosity < 2) ? PK_LOG_HEADERS :
                        ((verbosity < 3) ? PK_LOG_DATA :
                        ((verbosity < 4) ? PK_LOG_DEBUG :
                        ((verbosity < 5) ? (PK_LOG_DEBUG|PK_LOG_TRACE) :
                                           PK_LOG_ALL))))));
  }
  else {
    pk_state.log_mask = verbosity;
  }

  pk_state.use_ipv4 = (0 != (flags & PK_WITH_IPV4));
#ifdef HAVE_IPV6
  pk_state.use_ipv6 = (0 != (flags & PK_WITH_IPV6));
#endif
  pk_state.ddns_request_public_ipv4 = (0 != (flags & PK_WITH_IPV4_DNS));
  pk_state.ddns_request_public_ipv6 = (0 != (flags & PK_WITH_IPV6_DNS));

  if (flags & PK_WITH_SSL) {
    PKS_SSL_INIT(ssl_ctx);
  }
  else {
    ssl_ctx = NULL;
  }

  /* Note: This will leak memory if used. */
  if (app_id != NULL) pk_state.app_id_long = strdup(app_id);

#ifdef HAVE_SYSLOG_H
  if (flags & PK_WITH_SYSLOG) {
    openlog(pk_state.app_id_long, 0, LOG_USER);
    pk_state.log_file = NULL;
  }
#endif

  if (NULL == (pkm_ = pkm_manager_init(NULL, 0, NULL,
                                       max_kites,
                                       max_frontends,
                                       max_conns,
                                       (dyndns_url && *dyndns_url) ? dyndns_url : NULL,
                                       ssl_ctx))) {
    return NULL;
  }

  if (flags & PK_WITH_SERVICE_FRONTENDS) {
    if (0 > pagekite_add_service_frontends((pagekite_mgr) pkm_, flags)) {
      pagekite_free((pagekite_mgr) pkm_);
      return NULL;
    }
  }

  pkm_set_timer_enabled(pkm_, 1);
  pkm_tick(pkm_);

  return (pagekite_mgr) pkm_;
}

pagekite_mgr pagekite_init_pagekitenet(
  const char* app_id,
  int max_kites,
  int max_conns,
  int flags,
  int verbosity)
{
  pagekite_mgr pkm;

  if (flags & PK_WITH_DEFAULTS || !flags) flags |= PK_DEFAULT_FLAGS;

  if (NULL == (pkm = pagekite_init(app_id,
                                   max_kites,
                                   PAGEKITE_NET_FE_MAX,
                                   max_conns,
                                   PAGEKITE_NET_DDNS,
                                   flags, verbosity)))
  {
    return NULL;
  }

  /* If flags say anything about service frontends, do nothing: either
     it was already done, or we're not supposed to do anything. */
  if (!(flags & (PK_WITHOUT_SERVICE_FRONTENDS|PK_WITH_SERVICE_FRONTENDS))) {
    if (0 > pagekite_add_service_frontends(pkm, flags)) {
      pagekite_free(pkm);
      return NULL;
    }
  }

  return pkm;
}

pagekite_mgr pagekite_init_whitelabel(
  const char* app_id,
  int max_kites,
  int max_conns,
  int flags,
  int verbosity,
  const char* whitelabel_tld)
{
  char cert_name[256];
  char dynamic_dns_url[256];
  pagekite_mgr pkm;

  if (flags & PK_WITH_DEFAULTS || !flags) flags |= PK_DEFAULT_FLAGS;

  if (whitelabel_tld) {
    sprintf(dynamic_dns_url, PAGEKITE_NET_WL_DDNS, whitelabel_tld);
  }
  else {
    strcpy(dynamic_dns_url, PAGEKITE_NET_DDNS);
  }

  /* Make sure nothing at all is done about frontends in the default
   * init routine, we're taking care of that ourselves. */
  int init_flags = flags;
  init_flags &= ~PK_WITH_SERVICE_FRONTENDS;
  init_flags |= PK_WITHOUT_SERVICE_FRONTENDS;

  if (NULL == (pkm = pagekite_init(app_id,
                                   max_kites,
                                   PAGEKITE_NET_FE_MAX,
                                   max_conns,
                                   dynamic_dns_url,
                                   init_flags,
                                   verbosity)))
  {
    return NULL;
  }

  /* Add to our list of acceptable certificate names */
  if (whitelabel_tld) {
    char* cert_names[2];
    cert_names[0] = cert_name;
    cert_names[1] = NULL;
    sprintf(cert_name, "fe.%.250s", whitelabel_tld);
    pks_add_ssl_cert_names(cert_names);
    if (!(flags & PK_WITH_FRONTEND_SNI)) {
      /* Adding multiple names disables SNI, but lets whitelabels
       * use SSL even if they haven't got a cert on the pagekite.net
       * relays since the default cert will be acceptable. */
      pks_add_ssl_cert_names(PAGEKITE_NET_CERT_NAMES);
    }
  }
  else {
    pks_add_ssl_cert_names(PAGEKITE_NET_CERT_NAMES);
  }

  /* If flags say anything about service frontends, do nothing: either
     it was already done, or we're not supposed to do anything. */
  if (!(flags & PK_WITHOUT_SERVICE_FRONTENDS)) {
    if (0 > pagekite_add_whitelabel_frontends(pkm, flags, whitelabel_tld)) {
      pagekite_free(pkm);
      return NULL;
    }
  }

  return pkm;
}

int pagekite_add_whitelabel_frontends(
  pagekite_mgr pkm,
  int flags,
  const char* whitelabel_tld)
{
  int fes_v4 = 0;
  char fdomain_v4[256];
#ifdef HAVE_IPV6
  int fes_v6 = 0;
  char fdomain_v6[256];
#endif
  int add_null_records;

  if (!whitelabel_tld)
    return pagekite_add_service_frontends(pkm, flags);

  if (flags & PK_WITH_DEFAULTS || !flags) flags |= PK_DEFAULT_FLAGS;
  add_null_records = (flags & PK_WITH_DYNAMIC_FE_LIST);

  sprintf(fdomain_v4, PAGEKITE_NET_WL_V4FRONTENDS, whitelabel_tld);
  if (flags & PK_WITH_IPV4) {
    fes_v4 = pkm_lookup_and_add_frontend(PK_MANAGER(pkm),
                                         fdomain_v4,
                                         PAGEKITE_NET_WL_FRONTEND_PORT,
                                         FE_STATUS_AUTO,
                                         add_null_records);
  }

#ifdef HAVE_IPV6
  sprintf(fdomain_v6, PAGEKITE_NET_WL_V6FRONTENDS, whitelabel_tld);
  if (flags & PK_WITH_IPV6) {
    fes_v6 = pkm_lookup_and_add_frontend(PK_MANAGER(pkm),
                                         fdomain_v6,
                                         PAGEKITE_NET_WL_FRONTEND_PORT,
                                         FE_STATUS_AUTO,
                                         add_null_records);
  }
  if ((fes_v4 < 0) && (fes_v6 < 0)) return -1;

  int fes = fes_v4 + fes_v6;
  if (fes_v4 < 0) {
    fes = fes_v6;
  }
  else if (fes_v6 < 0) {
    fes = fes_v4;
  }
#else
  if (fes_v4 < 0) return -1;
  int fes = fes_v4;
#endif

  if (0 == fes) {
    pk_set_error(ERR_NO_FRONTENDS);
    return -1;
  }
  return fes;
}

int pagekite_add_service_frontends(pagekite_mgr pkm, int flags) {
  int fes_v4 = 0;
#ifdef HAVE_IPV6
  int fes_v6 = 0;
#endif
  int add_null_records;

  if (flags & PK_WITH_DEFAULTS || !flags) flags |= PK_DEFAULT_FLAGS;
  add_null_records = (flags & PK_WITH_DYNAMIC_FE_LIST);

  if (flags & PK_WITH_IPV4) {
    fes_v4 = pkm_lookup_and_add_frontend(PK_MANAGER(pkm),
                                         PAGEKITE_NET_V4FRONTENDS,
                                         FE_STATUS_AUTO,
                                         add_null_records);
  }

#ifdef HAVE_IPV6
  if (flags & PK_WITH_IPV6) {
    fes_v6 = pkm_lookup_and_add_frontend(PK_MANAGER(pkm),
                                         PAGEKITE_NET_V6FRONTENDS,
                                         FE_STATUS_AUTO,
                                         add_null_records);
  }

  if ((fes_v6 < 0) && (fes_v4 < 0)) return -1;

  int fes = fes_v4 + fes_v6;
  if (fes_v4 < 0) {
    fes = fes_v6;
  }
  else if (fes_v6 < 0) {
    fes = fes_v4;
  }
#else
  if (fes_v4 < 0) return -1;
  int fes = fes_v4;
#endif

  /* Set/add the default SSL certificate names */
  if (pk_state.ssl_cert_names == NULL) {
    pk_state.ssl_cert_names = PAGEKITE_NET_CERT_NAMES;
  }
  else {
    pks_add_ssl_cert_names(PAGEKITE_NET_CERT_NAMES);
  }

  if (0 == fes) {
    pk_set_error(ERR_NO_FRONTENDS);
    return -1;
  }
  return fes;
}

int pagekite_free(pagekite_mgr pkm) {
  if (pkm == NULL) return -1;
#ifdef HAVE_OPENSSL
  if (PK_MANAGER(pkm)->ssl_ctx != NULL) SSL_CTX_free(PK_MANAGER(pkm)->ssl_ctx);
#endif
  pkm_manager_free(PK_MANAGER(pkm));
  pks_free_ssl_cert_names();
#ifdef _MSC_VER
  Sleep(100); /* Give logger time to get the rest of the log for debugging */
  WSACleanup();
#endif
  return 0;
}

int pagekite_set_log_mask(pagekite_mgr pkm, int mask)
{
  (void) pkm;
  pk_state.log_mask = mask;
  return 0;
}

int pagekite_set_log_destination(pagekite_mgr pkm, int log_destination)
{
  (void) pkm;
  if (log_destination == PK_LOG_DEST_SYSLOG) {
    pk_state.log_file = NULL;
  }
  else if (log_destination == PK_LOG_DEST_NONE) {
    pk_state.log_file = PK_DISABLE_LOGGING;
  }
  else {
    pk_state.log_file = fdopen(log_destination, "a");
  }
  return 0;
}

int pagekite_set_housekeeping_min_interval(pagekite_mgr pkm, int seconds)
{
  if (pkm == NULL) return -1;
  if (seconds < PK_HOUSEKEEPING_INTERVAL_MIN)
    seconds = PK_HOUSEKEEPING_INTERVAL_MIN;
  PK_MANAGER(pkm)->housekeeping_interval_min = seconds;
  return seconds;
}

int pagekite_set_housekeeping_max_interval(pagekite_mgr pkm, int seconds)
{
  if (pkm == NULL) return -1;
  if (seconds < PK_HOUSEKEEPING_INTERVAL_MAX_MIN)
    seconds = PK_HOUSEKEEPING_INTERVAL_MAX_MIN;
  PK_MANAGER(pkm)->housekeeping_interval_max = seconds;
  return 0;
}

int pagekite_set_rejection_url(pagekite_mgr pkm, const char* url)
{
  if (pkm == NULL) return -1;
  if (PK_MANAGER(pkm)->fancy_pagekite_net_rejection_url != PK_REJECT_FANCY_URL)
    free(PK_MANAGER(pkm)->fancy_pagekite_net_rejection_url);
  if (url == NULL) {
    PK_MANAGER(pkm)->fancy_pagekite_net_rejection_url = PK_REJECT_FANCY_URL;
  }
  else {
    PK_MANAGER(pkm)->fancy_pagekite_net_rejection_url = strdup(url);
  }
  return 0;
}

int pagekite_enable_watchdog(pagekite_mgr pkm, int enable)
{
  if (pkm == NULL) return -1;
  PK_MANAGER(pkm)->enable_watchdog = (enable > 0);
  return 0;
}

int pagekite_enable_http_forwarding_headers(pagekite_mgr pkm, int enable)
{
  if (pkm == NULL) return -1;
  PK_MANAGER(pkm)->enable_http_forwarding_headers = (enable > 0);
  return 0;
}

int pagekite_enable_fake_ping(pagekite_mgr pkm, int enable)
{
  (void) pkm;
  pk_state.fake_ping = enable;
  return 0;
}

int pagekite_set_bail_on_errors(pagekite_mgr pkm, int errors)
{
  (void) pkm;
  pk_state.bail_on_errors = errors;
  return 0;
}

int pagekite_set_conn_eviction_idle_s(pagekite_mgr pkm, int seconds)
{
  (void) pkm;
  pk_state.conn_eviction_idle_s = seconds;
  return 0;
}

int pagekite_set_openssl_ciphers(pagekite_mgr pkm, const char* ciphers)
{
  (void) pkm;
  if(pk_state.ssl_ciphers != NULL && pk_state.ssl_ciphers != PKS_DEFAULT_CIPHERS)
    free(pk_state.ssl_ciphers);
  pk_state.ssl_ciphers = strdup(ciphers);
  return 0;
}

int pagekite_want_spare_frontends(pagekite_mgr pkm, int spares)
{
  if (pkm == NULL) return -1;
  PK_MANAGER(pkm)->want_spare_frontends = spares;
  return 0;
}


int pagekite_add_kite(pagekite_mgr pkm,
  const char* proto,
  const char* kitename,
  int pport,
  const char* secret,
  const char* backend,
  int lport)
{
  if (pkm == NULL) return -1;
  return (NULL != pkm_add_kite(PK_MANAGER(pkm),
                               (proto && *proto) ? proto : NULL,
                               (kitename && *kitename) ? kitename : NULL,
                               pport,
                               (secret && *secret) ? secret : NULL,
                               (backend && *backend) ? backend : NULL,
                               lport)
          ) ? 0 : -1;
}

int pagekite_lookup_and_add_frontend(pagekite_mgr pkm,
  const char* domain,
  int port,
  int dns_updates)
{
  if (pkm == NULL) return -1;
  return pkm_lookup_and_add_frontend(PK_MANAGER(pkm), domain, port,
                                     FE_STATUS_AUTO, dns_updates);
}

int pagekite_add_frontend(pagekite_mgr pkm,
  const char* domain,
  int port)
{
  if (pkm == NULL) return -1;
  return pkm_lookup_and_add_frontend(PK_MANAGER(pkm), domain, port,
                                     FE_STATUS_AUTO, 0);
}

int pagekite_enable_tick_timer(pagekite_mgr pkm, int enabled)
{
  if (pkm == NULL) return -1;
  pkm_set_timer_enabled(PK_MANAGER(pkm), enabled);
  return 0;
}

int pagekite_tick(pagekite_mgr pkm)
{
  if (pkm == NULL) return -1;
  pkm_tick(PK_MANAGER(pkm));
  return 0;
}

int pagekite_poll(pagekite_mgr pkm, int timeout) {
  if (pkm == NULL) return -1;
  pthread_mutex_lock(&(pk_state.lock));
  /* FIXME: Obey the timeout */
  pthread_cond_wait(&(pk_state.cond), &(pk_state.lock));
  pthread_mutex_unlock(&(pk_state.lock));
  return 0;
}

int pagekite_thread_start(pagekite_mgr pkm) {
  if (pkm == NULL) return -1;
  return pkm_run_in_thread(PK_MANAGER(pkm));
}

int pagekite_thread_wait(pagekite_mgr pkm) {
  if (pkm == NULL) return -1;
  return pkm_wait_thread(PK_MANAGER(pkm));
}

int pagekite_thread_stop(pagekite_mgr pkm) {
  if (pkm == NULL) return -1;
  if (0 > pkm_stop_thread(PK_MANAGER(pkm))) return -1;
  return 0;
}

int pagekite_set_event_mask(pagekite_mgr pkm, unsigned int mask) {
  if (pkm == NULL) return -1;
  PK_MANAGER(pkm)->events.event_mask = mask;
  return 0;
}

unsigned int pagekite_await_event(pagekite_mgr pkm, int timeout) {
  if (pkm == NULL) return -1;
  return pke_await_event(&(PK_MANAGER(pkm)->events), timeout)->event_code;
}

int pagekite_get_event_int(pagekite_mgr pkm, unsigned int event_code)
{
  if (pkm == NULL) return 0;
  return pke_get_event(&(PK_MANAGER(pkm)->events), event_code)->event_int;
}

const char* pagekite_get_event_str(pagekite_mgr pkm, unsigned int event_code)
{
  /* We always return a string, but it may be the empty string. */
  if (pkm == NULL) return "";
  const char* s =  pke_get_event(&(PK_MANAGER(pkm)->events),
                                 event_code)->event_str;
  return (s == NULL) ? "" : s;
}

int pagekite_event_respond(pagekite_mgr pkm,
  unsigned int event_code,
  unsigned int response_code)
{
  if (pkm == NULL) return 0;
  pke_post_response(&(PK_MANAGER(pkm)->events),
                    event_code, response_code, 0, NULL);
  return 0;
}

int pagekite_event_respond_with_data(
  pagekite_mgr pkm,
  unsigned int event_code,
  unsigned int response_code,
  int          response_int,
  const char*  response_str)
{
  if (pkm == NULL) return 0;
  pke_post_response(&(PK_MANAGER(pkm)->events),
                    event_code, response_code, response_int, response_str);
  return 0;
}

int pagekite_get_status(pagekite_mgr pkm) {
  if (pkm == NULL) return -1;
  return PK_MANAGER(pkm)->status;
}

int pagekite_perror(pagekite_mgr pkm, const char* prefix) {
  (void) pkm;
  pk_perror(prefix);
  return 0;
}

char* pagekite_get_log(pagekite_mgr pkm) {
  static char buffer[PKS_LOG_DATA_MAX+1];
  if (pkm == NULL) {
    strcpy(buffer, "Not running.");
  }
  else {
    pks_copylog(buffer);
  }
  buffer[PKS_LOG_DATA_MAX] = '\0';
  return buffer;
}

int pagekite_dump_state_to_log(pagekite_mgr pkm)
{
  pk_dump_state(PK_MANAGER(pkm));
  return 0;
}
