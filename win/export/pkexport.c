/******************************************************************************
pkexport.c - A wrapper for building the libpagekite library as a dll.

*******************************************************************************

This file is Copyright 2012-2013, The Beanstalks Project ehf.

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

/* this file is a work in progress */

#include "pkexport.h"

#define BUFFER_SIZE 512 * 1024
struct pk_manager *pk_manager_global = NULL;
char pk_manager_buffer[BUFFER_SIZE];

int libpagekite_init(int kites, int max_conns, int static_setup, 
  int spare_frontends, int verbosity) 
{
  int r;
  char* ddns_url;
  SSL_CTX* ssl_ctx;
  WSADATA wsa_data;

  srand(time(0) ^ getpid());
  pks_global_init(PK_LOG_NORMAL);

  pk_state.log_mask = ((verbosity < 0) ? PK_LOG_ERRORS :
                      ((verbosity < 1) ? PK_LOG_NORMAL :
                      ((verbosity < 2) ? PK_LOG_DEBUG : PK_LOG_ALL)));

  PKS_SSL_INIT(ssl_ctx);

  if (0 != (r = WSAStartup(MAKEWORD(2, 2), &wsa_data))) {
    pk_log(PK_LOG_ERROR, "libpagekite: Error during WSAStartup: %d\n", r);
    return -1;
  }

  if (static_setup) {
    ddns_url = NULL;
  }
  else {
    ddns_url = PAGEKITE_NET_DDNS;
  }

  pk_state.app_id_short = "libpagekite";
  pk_state.app_id_long = PK_VERSION;

  if (NULL == (pk_manager_global = pkm_manager_init(NULL, BUFFER_SIZE, 
                                                    pk_manager_buffer, 
	                                                kites, PAGEKITE_NET_FE_MAX,
													max_conns, ddns_url, ssl_ctx))) {
    pk_log(PK_LOG_ERROR, "libpagekite: Error initializing pk_manager");
	return -1;
  }

  pk_manager_global->want_spare_frontends = spare_frontends;

  if (ddns_url != NULL) {
    if ((0 > (pkm_add_frontend(pk_manager_global, 
                               PAGEKITE_NET_V4FRONTENDS, FE_STATUS_AUTO)))
#ifdef HAVE_IPV6
        || 0 > (pkm_add_frontend(pk_manager_global, 
                                 PAGEKITE_NET_V6FRONTENDS, FE_STATUS_AUTO)))
#endif
        ) {
      pk_manager_global = NULL;
      pk_log(PK_LOG_ERROR, "libpagekite: Error adding frontends");
      return -1;
    }
  }

  pkm_set_timer_enabled(pk_manager_global, 0);
  pkm_tick(pk_manager_global);

  return 0;
}

int libpagekite_useEvil() {
  pk_manager_global->housekeeping_interval_min = 5;
  pk_manager_global->housekeeping_interval_max = 20;
  pk_manager_global->check_world_interval = 30;
}

int libpagekite_useWatchdog() {
  pk_manager_global->enable_watchdog = 1;
}

int libpagekite_addKite(char* proto, char* kitename, int pport, 
  char* secret, char* backend, int lport) 
{
  if (pk_manager_global == NULL) return -1;

  pk_log(PK_LOG_MANAGER_DEBUG, "libpagekite: Adding kite: %s://%s:%d -> %s:%d",
                               proto, kitename, pport, backend, lport);

  if (NULL == pkm_add_kite(pk_manager_global, proto, kitename, pport, 
                           secret, backend, lport)) {
    pk_log(PK_LOG_ERROR, "libpagekite: Error adding kite");
	return -1;
  }

  return 0;
}

int libpagekite_addFrontend(char* domain, int port) {

  if(pk_manager_global == NULL) return -1;

  pk_log(PK_LOG_MANAGER_DEBUG, "libpagekite: Adding frontend: %s:%d", domain, port);

  if (0 > pkm_add_frontend(pk_manager_global, domain, port, FE_STATUS_AUTO)) {
    pk_log(PK_LOG_ERROR, "libpagekite: Error adding frontend");
    return -1;
  }

  return 0;
}

int libpagekite_tick() {
  if (pk_manager_global == NULL) return -1;
  
  pkm_tick(pk_manager_global);

  return 0;
}

int libpagekite_poll(int timeout) {
  if (pk_manager_global == NULL) return -1;

  pthread_mutex_lock(&(pk_state.lock));
  /* FIXME: Obey the timeout */
  pthread_cond_wait(&(pk_state.cond), &(pk_state.lock));
  pthread_mutex_unlock(&(pk_state.lock));

  return 0;
}

int libpagekite_start() {
  if (pk_manager_global == NULL) return -1;

  pk_log(PK_LOG_MANAGER_DEBUG, "libpagekite: Starting worker thread");

  if (0 > pkm_run_in_thread(pk_manager_global)) {
    pk_log(PK_LOG_ERROR, "libpagekite: Error starting worker thread");
    return -1;
  }

  return 0;
}

int libpagekite_stop() {
  if (pk_manager_global == NULL) return -1;

  pk_log(PK_LOG_MANAGER_DEBUG, "libpagekite: Stopping worker thread");

  if (0 > pkm_stop_thread(pk_manager_global)) {
    pk_log(PK_LOG_ERROR, "libpagekite: Error stopping worker thread");
    return -1;
  }

  WSACleanup();
  pk_manager_global = NULL;
  return 0;
}

int libpagekite_getStatus() {
  if (pk_manager_global == NULL) return -1;
  return pk_manager_global->status;
}

char* libpagekite_getLog() {
  char buffer[PKS_LOG_DATA_MAX];
  
  if (pk_manager_global == NULL) {
    strcpy(buffer, "Not running.");
  }
  else {
    pks_copylog(buffer);
  }

  return buffer;
}