/******************************************************************************
pkexport.c - A wrapper for building the libpagekite library as dll.

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

/* THIS FILE IS A WORK IN PROGRESS */

#include "pkexport.h"

struct pk_manager *pk_manager_global = NULL;
char pk_app_id_short[128];
char pk_app_id_long[128];

int pagekitec_init(int kites, int debug) {
  int r;
  int max_conns = 25;
  char* ddns_url = PAGEKITE_NET_DDNS;
  SSL_CTX* ssl_ctx;
  WSADATA wsa_data;

  if (debug)
    pks_global_init(PK_LOG_ALL);
  else
    pks_global_init(PK_LOG_NORMAL);

  PKS_SSL_INIT(ssl_ctx);

  if (0 != (r = WSAStartup(MAKEWORD(2, 2), &wsa_data))) {
    pk_log(PK_LOG_ERROR, "pagekitec: WSAStartup failed: %d\n", r);
    return -1;
  }

  pk_state.app_id_short = pk_app_id_short;
  pk_state.app_id_long = pk_app_id_long;

  if (NULL == (pk_manager_global = pkm_manager_init(NULL, 0, NULL, 
	                                                kites, PAGEKITE_NET_FE_MAX,
													max_conns, ddns_url, ssl_ctx))) {
    pk_log(PK_LOG_ERROR, "pagekitec: Failed initializing pk_manager");
	return -1;
  }

  if ((0 > (pkm_add_frontend(pk_manager_global, 
	                         PAGEKITE_NET_V4FRONTENDS, FE_STATUS_AUTO)))
#ifdef HAVE_IPV6
      || 0 > (pkm_add_frontend(pk_manager_global, 
	                           PAGEKITE_NET_V6FRONTENDS, FE_STATUS_AUTO)))
#endif
      ) {
    pk_manager_global = NULL;
	pk_log(PK_LOG_ERROR, "pagekitec: Failed adding frontends");
	return -1;
  }

  pkm_set_timer_enabled(pk_manager_global, 0);
  pkm_tick(pk_manager_global);

  return 0;
}

int pagekitec_addKite(char* proto, char* kitename, int pport, 
	                  char* secret, char* backend, int lport) {
  if (pk_manager_global == NULL) return -1;

  pk_log(PK_LOG_MANAGER_DEBUG, "pagekitec: Add kite: %s://%s:%d -> %s:%d",
                               proto, kitename, pport, backend, lport);

  if (NULL == pkm_add_kite(pk_manager_global, proto, kitename, pport, 
                           secret, backend, lport)) {
    pk_log(PK_LOG_ERROR, "pagekitec: Failed adding kite");
	return -1;
  }

  return 0;
}

int pagekitec_addFrontend(char* domain, int port) {

  if(pk_manager_global == NULL) return -1;

  pk_log(PK_LOG_MANAGER_DEBUG, "pagekitec: Add frontend: %s:%d", domain, port);

  if (0 > pkm_add_frontend(pk_manager_global, domain, port, FE_STATUS_AUTO)) {
    pk_log(PK_LOG_ERROR, "pagekitec: Failed adding frontend");
    return -1;
  }

  return 0;
}

int pagekitec_start() {
  if (pk_manager_global == NULL) return -1;

  pk_log(PK_LOG_MANAGER_DEBUG, "pagekitec: Starting worker thread");

  if (0 > pkm_run_in_thread(pk_manager_global)) {
    pk_log(PK_LOG_ERROR, "pagekitec: Error starting worker thread");
    return -1;
  }

  return 0;
}

int pagekitec_stop() {
  if (pk_manager_global == NULL) return -1;

  pk_log(PK_LOG_MANAGER_DEBUG, "pagekitec: Stopping worker thread");

  if (0 > pkm_stop_thread(pk_manager_global)) {
    pk_log(PK_LOG_ERROR, "pagekitec: Error stopping worker thread");
    return -1;
  }

  WSACleanup(); //maybe?
  pk_manager_global = NULL;
  return 0;
}