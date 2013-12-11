/******************************************************************************
pagekitec.c - A high-performance PageKite connector in C.

Usage: pagekitec [options] LPORT PROTO NAME.pagekite.me PPORT SECRET ...

*******************************************************************************

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
#include "pkstate.h"
#include "pkerror.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"
#include "version.h"

#include "pagekite_net.h"

#include <signal.h>


void usage(int ecode) {
  fprintf(stderr, "This is pagekitec.c from libpagekite %s.\n\n", PK_VERSION);
  fprintf(stderr, "Usage:\tpagekitec [options] LPORT PROTO"
                                   " NAME.pagekite.me PPORT SECRET ...\n"
                  "Options:\n"
                  "\t-q\tDecrease verbosity (less log output)\n"
                  "\t-v\tIncrease verbosity (more log output)\n"
                  "\t-I\tConnect insecurely, without SSL.\n"
                  "\t-S\tStatic setup, disable FE failover and DDNS updates\n"
                  "\t-c N\tSet max connection count to N (default = 25)\n"
                  "\t-n N\tAlways connect to N spare frontends (default = 0)\n"
                  "\t-B N\tBail out (abort) after N logged errors\n"
                  "\t-E N\tAllow eviction of streams idle for >N seconds\n"
                  "\t-F x\tUse x (a DNS name) as frontend pool\n"
                  "\t-R\tChoose frontends at random, instead of pinging\n"
                  "\t-4\tDisable IPv4 frontends\n"
                  "\t-6\tDisable IPv6 frontends\n"
                  "\t-C\tDisable auto-adding current DNS IP as a front-end\n"
                  "\t-Z\tEvil mode: Very low intervals for reconnect/ping\n"
                  "\n");
  exit(ecode);
}

void raise_log_level(int sig) {
  if (sig) pk_state.log_mask = PK_LOG_ALL;
}

int main(int argc, char **argv) {
  struct pk_manager *m;
  unsigned int tmp_uint;
  char* proto;
  char* kitename;
  char* secret;
  int gotargs = 0;
  int verbosity = 0;
  int use_ipv4 = 1;
  int use_ipv6 = 1;
  int use_current = 1;
  int use_ssl = 1;
  int use_evil = 0;
  int max_conns = 25;
  int spare_frontends = 0;
  char* fe_hostname = NULL;
  char* ddns_url = PAGEKITE_NET_DDNS;
  int ac;
  int pport;
  int lport;
  SSL_CTX* ssl_ctx;

  /* FIXME: Is this too lame? */
  srand(time(0) ^ getpid());
  pks_global_init(PK_LOG_NORMAL);

  while (-1 != (ac = getopt(argc, argv, "46c:B:CE:F:In:qRSvZ"))) {
    switch (ac) {
      case '4':
        use_ipv4 = 0;
        break;
      case '6':
        use_ipv6 = 0;
        break;
      case 'C':
        use_current = 0;
        break;
      case 'v':
        verbosity++;
        break;
      case 'q':
        verbosity--;
        break;
      case 'I':
        use_ssl = 0;
        break;
      case 'R':
        pk_state.fake_ping = 1;
        break;
      case 'S':
        ddns_url = NULL;
        break;
      case 'Z':
        use_evil = 1;
        break;
      case 'F':
        gotargs++;
        fe_hostname = strdup(optarg);
        break;
      case 'B':
        gotargs++;
        if (1 == sscanf(optarg, "%u", &pk_state.bail_on_errors)) break;
        usage(1);
      case 'c':
        gotargs++;
        if (1 == sscanf(optarg, "%d", &max_conns)) break;
        usage(1);
      case 'E':
        gotargs++;
        if (1 == sscanf(optarg, "%u", &tmp_uint)) {
          pk_state.conn_eviction_idle_s = tmp_uint;
          break;
        }
        usage(1);
      case 'n':
        gotargs++;
        if (1 == sscanf(optarg, "%d", &spare_frontends)) break;
        usage(1);
      default:
        usage(1);
    }
    gotargs++;
  }

  if ((argc-1-gotargs) < 5 || ((argc-1-gotargs) % 5) != 0) usage(1);

  signal(SIGUSR1, &raise_log_level);
  pk_state.log_mask = ((verbosity < 0) ? PK_LOG_ERRORS :
                      ((verbosity < 1) ? PK_LOG_NORMAL :
                      ((verbosity < 2) ? PK_LOG_DEBUG : PK_LOG_ALL)));

  if (use_ssl) {
    PKS_SSL_INIT(ssl_ctx);
  }
  else {
    ssl_ctx = NULL;
  }

  if (NULL == (m = pkm_manager_init(NULL, 0, NULL,
                                    1 + (argc-1-gotargs)/5, /* Kites */
                                    PAGEKITE_NET_FE_MAX,
                                    max_conns, ddns_url, ssl_ctx))) {
    pk_perror(argv[0]);
    exit(1);
  }
  m->want_spare_frontends = spare_frontends;
  if (use_evil) {
    m->housekeeping_interval_min = 5;
    m->housekeeping_interval_max = 20;
    m->check_world_interval = 30;
  }

  for (ac = gotargs; ac+5 < argc; ac += 5) {
    if ((1 != sscanf(argv[ac+1], "%d", &lport)) ||
        (1 != sscanf(argv[ac+4], "%d", &pport))) {
      usage(2);
    }
    proto = argv[ac+2];
    kitename = argv[ac+3];
    secret = argv[ac+5];
    if ((NULL == (pkm_add_kite(m, proto, kitename, 0, secret,
                               "localhost", lport))) ||
        (use_current &&
         (0 > (pkm_add_frontend(m, kitename, pport, FE_STATUS_AUTO))))) {
      pk_perror(argv[0]);
      exit(3);
    }
  }

  if (ddns_url != NULL) {
    if (fe_hostname) {
      if (0 > (pkm_add_frontend(m, fe_hostname, 443, FE_STATUS_AUTO))) {
        pk_perror(argv[0]);
        exit(4);
      }
    }
    else if (((use_ipv4) &&
              (0 > (pkm_add_frontend(m, PAGEKITE_NET_V4FRONTENDS, FE_STATUS_AUTO)))) ||
             ((use_ipv6) &&
              (0 > (pkm_add_frontend(m, PAGEKITE_NET_V6FRONTENDS, FE_STATUS_AUTO))))) {
      pk_perror(argv[0]);
      exit(4);
    }
  }

  if (0 > pkm_run_in_thread(m)) {
    pk_perror(argv[0]);
    exit(5);
  }

  pkm_wait_thread(m);
  return 0;
}

