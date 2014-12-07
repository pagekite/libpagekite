/******************************************************************************
pagekiter.c - A high-performance PageKite relay in C.

Usage: pagekiter [options] PORT PORT ... NAME.pagekite.me SECRET ...

*******************************************************************************

This file is Copyright 2011-2014, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify it under
the terms of the  GNU Affero General Public License, version 3.0 or above, as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the Apache License for more details.

You should have received a copy of the GNU AGPL along with this program.
If not, see: <http://www.gnu.org/licenses/agpl.html>

Note: For alternate license terms, see the file COPYING.md.

******************************************************************************/

#include "common.h"
#include "pagekite.h"
#include <signal.h>

#include "utils.h"
#include "pkstate.h"
#include "pkerror.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pkrelay.h"
#include "pklogging.h"


#define EXIT_ERR_MANAGER_INIT 1
#define EXIT_ERR_USAGE 2
#define EXIT_ERR_ADD_LPORT 3
#define EXIT_ERR_ADD_KITE 4
#define EXIT_ERR_START_THREAD 5


void usage(int ecode) {
  fprintf(stderr, "This is pagekiter.c from libpagekite %s.\n\n", PK_VERSION);
  fprintf(stderr, "Usage:\tpagekiter ...\n"
                  "Options:\n"
                  "\t-q\tDecrease verbosity (less log output)\n"
                  "\t-v\tIncrease verbosity (more log output)\n"
                  "\t-c N\tSet max connection count to N (default = 25)\n"
                  "\t-a X\tUse domain X for DNS-based kite auth\n"
                  "\t-B N\tBail out (abort) after N logged errors\n"
                  "\t-E N\tAllow eviction of streams idle for >N seconds\n"
                  "\t-4\tDisable IPv4 listeners\n");
#ifdef HAVE_IPV6
  fprintf(stderr, "\t-6\tDisable IPv6 listeners\n");
#endif
  fprintf(stderr, "\t-W\tEnable watchdog thread (dumps core if we lock up)\n"
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
  int gotargs = 0;
  int verbosity = 0;
  int use_ipv4 = 1;
#ifdef HAVE_IPV6
  int use_ipv6 = 1;
#endif
  int use_evil = 0;
  int use_watchdog = 0;
  int max_conns = 25;
  int lport;
  int ac;
  SSL_CTX* ssl_ctx;

  /* FIXME: Is this too lame? */
  srand(time(0) ^ getpid());
  pks_global_init(PK_LOG_NORMAL);

  while (-1 != (ac = getopt(argc, argv, "46a:c:B:E:qvWZ"))) {
    switch (ac) {
      case '4':
        use_ipv4 = 0;
        break;
      case '6':
#ifdef HAVE_IPV6
        use_ipv6 = 0;
#endif
        break;
      case 'v':
        verbosity++;
        break;
      case 'q':
        verbosity--;
        break;
      case 'W':
        use_watchdog = 1;
        break;
      case 'Z':
        use_evil = 1;
        break;
      case 'B':
        gotargs++;
        if (1 == sscanf(optarg, "%u", &pk_state.bail_on_errors)) break;
        usage(EXIT_ERR_USAGE);
      case 'c':
        gotargs++;
        if (1 == sscanf(optarg, "%d", &max_conns)) break;
        usage(EXIT_ERR_USAGE);
      case 'E':
        gotargs++;
        if (1 == sscanf(optarg, "%u", &tmp_uint)) {
          pk_state.conn_eviction_idle_s = tmp_uint;
          break;
        }
        usage(EXIT_ERR_USAGE);
      default:
        usage(EXIT_ERR_USAGE);
    }
    gotargs++;
  }

  if ((argc-1-gotargs) < 5 || ((argc-1-gotargs) % 5) != 0) {
    usage(EXIT_ERR_USAGE);
  }

  signal(SIGUSR1, &raise_log_level);

  pk_state.log_mask = ((verbosity < 0) ? PK_LOG_ERRORS :
                      ((verbosity < 1) ? PK_LOG_NORMAL :
                      ((verbosity < 2) ? PK_LOG_DEBUG : PK_LOG_ALL)));

  PKS_SSL_INIT(ssl_ctx);

  if (NULL == (m = pkm_manager_init(NULL, 0, NULL, 0,
                                    PAGEKITE_NET_CLIENT_MAX,
                                    max_conns, NULL, ssl_ctx))) {
    pk_perror(argv[0]);
    exit(EXIT_ERR_MANAGER_INIT);
  }
  if (use_evil) {
    m->housekeeping_interval_min = 5;
    m->housekeeping_interval_max = 20;
    m->check_world_interval = 30;
  }
  if (use_watchdog)
    m->enable_watchdog = 1;

  for (ac = gotargs; ac < argc; ac += 1) {
    if (1 == sscanf(argv[ac+1], "%d", &lport)) {
      if (use_ipv4)
        if (NULL == (pkr_add_listener(m, lport))) {
          pk_perror(argv[0]);
          exit(EXIT_ERR_ADD_LPORT);
        }
#ifdef HAVE_IPV6
      if (use_ipv6)
        if (NULL == (pkr_add_listener_v6(m, lport))) {
          pk_perror(argv[0]);
          exit(EXIT_ERR_ADD_LPORT);
        }
#endif
    } else {
      break;
    }
  }

  for (; ac+2 < argc; ac += 2) {
    if (NULL == (pkm_add_kite(m, NULL, argv[ac+1], 0, argv[ac+2], NULL, 0))) {
      pk_perror(argv[0]);
      exit(EXIT_ERR_ADD_KITE);
    }
  }

  if (0 > pkm_run_in_thread(m)) {
    pk_perror(argv[0]);
    exit(EXIT_ERR_START_THREAD);
  }

  pkm_wait_thread(m);

  return 0;
}
