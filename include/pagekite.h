/******************************************************************************
pagekite.h - The public library interface of libpagekite

*******************************************************************************

This file is Copyright 2012-2014, The Beanstalks Project ehf.

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

#ifndef _PAGEKITEC_DLL_H
#define _PAGEKITEC_DLL_H

#ifdef ANDROID
#define PK_VERSION "141208A"
#else
#define PK_VERSION "141208C"
#endif

/* Flags for libpagekite_init and friends. */
#define PK_WITH_SSL                  0x0001
#define PK_WITH_IPV4                 0x0002
#define PK_WITH_IPV6                 0x0004
#define PK_WITH_SERVICE_FRONTENDS    0x0008
#define PK_WITHOUT_SERVICE_FRONTENDS 0x0010

/* PageKite logging constants */
#define PK_LOG_TUNNEL_DATA     0x0001
#define PK_LOG_TUNNEL_HEADERS  0x0002
#define PK_LOG_TUNNEL_CONNS    0x0004
#define PK_LOG_BE_DATA         0x0010
#define PK_LOG_BE_HEADERS      0x0020
#define PK_LOG_BE_CONNS        0x0040
#define PK_LOG_MANAGER_ERROR   0x0100
#define PK_LOG_MANAGER_INFO    0x0200
#define PK_LOG_MANAGER_DEBUG   0x0400

#define PK_LOG_TRACE           0x0800
#define PK_LOG_ERROR           0x1000

#define PK_LOG_ERRORS          (PK_LOG_ERROR|PK_LOG_MANAGER_ERROR)
#define PK_LOG_MANAGER         (PK_LOG_MANAGER_ERROR|PK_LOG_MANAGER_INFO)
#define PK_LOG_CONNS           (PK_LOG_BE_CONNS|PK_LOG_TUNNEL_CONNS)
#define PK_LOG_NORMAL          (PK_LOG_ERRORS|PK_LOG_CONNS|PK_LOG_MANAGER)
#define PK_LOG_DEBUG           (PK_LOG_NORMAL|PK_LOG_MANAGER_DEBUG)
#define PK_LOG_ALL             0xffff

/* Pagekite.net service related constants */
#define PAGEKITE_NET_DDNS "http://up.pagekite.net/?hostname=%s&myip=%s&sign=%s"
#define PAGEKITE_NET_V4FRONTENDS "frontends.b5p.us", 443
#define PAGEKITE_NET_V6FRONTENDS "v6frontends.b5p.us", 443
#define PAGEKITE_NET_CLIENT_MAX 10000
#define PAGEKITE_NET_LPORT_MAX 1000
#define PAGEKITE_NET_FE_MAX 25


#ifndef PAGEKITE_CONSTANTS_ONLY
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#ifdef BUILDING_PAGEKITE_DLL
#define DECLSPEC_DLL __declspec(dllexport)
#else
#define DECLSPEC_DLL __declspec(dllimport)
#endif
#else
#define DECLSPEC_DLL 
#endif

typedef void* pagekite_mgr;

DECLSPEC_DLL pagekite_mgr libpagekite_init(
  const char* app_id_short,
  const char* app_id_long,
  int max_kites,
  int max_frontends,
  int max_conns,
  const char* dyndns_url,
  int flags,
  int verbosity);

DECLSPEC_DLL pagekite_mgr libpagekite_init_pagekitenet(
  const char* app_id_short,
  const char* app_id_long,
  int max_kites,
  int max_conns,
  int flags,
  int verbosity);

DECLSPEC_DLL int libpagekite_add_kite(pagekite_mgr,
  const char* proto,
  const char* kitename,
  int pport,
  const char* secret,
  const char* backend,
  int lport);

DECLSPEC_DLL int libpagekite_add_service_frontends(pagekite_mgr pkm, int);
DECLSPEC_DLL int libpagekite_add_frontend(pagekite_mgr,
  char* domain,
  int port);

DECLSPEC_DLL int libpagekite_set_log_mask(pagekite_mgr, int);
DECLSPEC_DLL int libpagekite_enable_watchdog(pagekite_mgr, int enable);
DECLSPEC_DLL int libpagekite_enable_fake_ping(pagekite_mgr pkm, int enable);
DECLSPEC_DLL int libpagekite_set_bail_on_errors(pagekite_mgr pkm, int errors);
DECLSPEC_DLL int libpagekite_set_conn_eviction_idle_s(pagekite_mgr pkm, int);
DECLSPEC_DLL int libpagekite_want_spare_frontends(pagekite_mgr, int spares);
DECLSPEC_DLL int libpagekite_tick(pagekite_mgr);
DECLSPEC_DLL int libpagekite_poll(pagekite_mgr, int timeout);
DECLSPEC_DLL int libpagekite_start(pagekite_mgr);
DECLSPEC_DLL int libpagekite_wait(pagekite_mgr);
DECLSPEC_DLL int libpagekite_stop(pagekite_mgr);
DECLSPEC_DLL int libpagekite_get_status(pagekite_mgr);
DECLSPEC_DLL char* libpagekite_get_log(pagekite_mgr);
DECLSPEC_DLL int libpagekite_free(pagekite_mgr);
DECLSPEC_DLL void libpagekite_perror(pagekite_mgr, const char*);

#ifdef __cplusplus
}
#endif
#endif
#endif
