/******************************************************************************
pagekiter.c - A high-performance PageKite front-end relay in C.

Usage: pagekiter [options] PROTO NAME.foo.com SECRET ...

*******************************************************************************

This file is Copyright 2015-2017, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published
by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

******************************************************************************/

#include "config.h"

#define WITH_PAGEKITE_RELAY 1
#include <pagekite.h>
#include "common.h"

#define EXIT_ERR_MANAGER_INIT 1
#define EXIT_ERR_USAGE 2
#define EXIT_ERR_ADD_KITE 3
#define EXIT_ERR_LISTEN 4
#define EXIT_ERR_START_THREAD 5
#define MAX_PLUGIN_ARGS 128
#define MAX_LISTEN_PORTS 128

pagekite_mgr m;


void usage(int ecode) {
  fprintf(stderr, "This is pagekiter.c from libpagekite %s.\n\n", PK_VERSION);
  fprintf(stderr, "Usage:\tpagekiter [options] PROTO foo.domain.com SECRET\n"
                  "Options:\n"
                  "\t-q\tDecrease verbosity (less log output)\n"
                  "\t-v\tIncrease verbosity (more log output)\n"
                  "\t-p N\tListen on for traffic on port N (default=9443)\n"
                  "\t-k N\tAllocate buffers for up to N kites\n"
                  "\t-c N\tAllocate buffers for up to N connections\n");
#ifdef HAVE_OPENSSL
  fprintf(stderr, "\t-I\tRun insecurely, disable SSL/TLS.\n");
#endif
  fprintf(stderr, "\t-4\tDisable IPv4\n");
#ifdef HAVE_IPV6
  fprintf(stderr, "\t-6\tDisable IPv6\n");
#endif
  fprintf(stderr, "\t-W\tEnable watchdog thread (dumps core if we lock up)\n");
#ifdef HAVE_LUA
  fprintf(stderr, "\t-o S=N\tEnable Lua plugin S with argument N\n"
                  "\t-L\tDisable by default all Lua plugins\n");
#endif
  fprintf(stderr, "\n");
  exit(ecode);
}

void raise_log_level(int sig) {
  if (sig) pagekite_set_log_mask(NULL, PK_LOG_ALL);
}

void shutdown_pagekite(int sig) {
  if (sig) pagekite_thread_stop(m);
}

void safe_exit(int code) {
#ifdef _MSC_VER
  fprintf(stderr, "Exiting with status code %d.\n", code);
#endif
  fclose(stderr);
  exit(code);
}

int main(int argc, char **argv) {
  unsigned int bail_on_errors = 0;
  unsigned int conn_eviction_idle_s = 0;
  char* proto;
  char* kitename;
  char* secret;
  char* lua_settings[MAX_PLUGIN_ARGS+1];
  int lua_settingc = 0;
  int lua_no_defaults = 0;
  int listen_ports[MAX_LISTEN_PORTS+1];
  int listen_portc = 0;
  int gotargs = 0;
  int verbosity = 0;
  int use_watchdog = 0;
  int init_kites = 0;
  int init_conns = 0;
  int ac;
  int flags = (PK_WITH_SSL
              |PK_WITH_IPV4
              |PK_WITH_SRAND_RESEED
              |PK_AS_FRONTEND_RELAY);
#ifdef HAVE_IPV6
  flags |= PK_WITH_IPV6;
#endif

  while (-1 != (ac = getopt(argc, argv, "46Ic:k:Lo:p:qvW"))) {
    switch (ac) {
      case '4':
        flags &= ~PK_WITH_IPV4;
        break;
#ifdef HAVE_IPV6
      case '6':
        flags &= ~PK_WITH_IPV6;
        break;
#endif
      case 'I':
        flags &= ~PK_WITH_SSL;
        break;
      case 'c':
        if ((1 != sscanf(optarg, "%d", &init_conns)) || (init_conns < 1)) {
          usage(EXIT_ERR_USAGE);
        }
        gotargs++;
        break;
      case 'k':
        if ((1 != sscanf(optarg, "%d", &init_kites)) || (init_kites < 1)) {
          usage(EXIT_ERR_USAGE);
        }
        gotargs++;
        break;
      case 'L':
        lua_no_defaults = 1;
        break;
      case 'o':
        if (lua_settingc >= MAX_PLUGIN_ARGS) usage(EXIT_ERR_USAGE);
        gotargs++;
        lua_settings[lua_settingc++] = strdup(optarg);
        break;
      case 'p':
        if ((listen_portc >= MAX_LISTEN_PORTS) ||
            (1 != sscanf(optarg, "%d", &listen_ports[listen_portc++]))) {
          usage(EXIT_ERR_USAGE);
        }
        gotargs++;
        break;
      case 'q':
        verbosity--;
        break;
      case 'v':
        verbosity++;
        break;
      case 'W':
        use_watchdog = 1;
        break;
      default:
        usage(EXIT_ERR_USAGE);
    }
    gotargs++;
  }
  lua_settings[lua_settingc] = NULL;

  /* By default we listen on port 9443 */
  if (listen_portc == 0) listen_ports[listen_portc++] = 9443;

  if ((argc-1-gotargs) < 3 || ((argc-1-gotargs) % 3) != 0) {
    usage(EXIT_ERR_USAGE);
  }

#ifndef _MSC_VER
  signal(SIGUSR1, &raise_log_level);
  signal(SIGQUIT, &shutdown_pagekite);
  signal(SIGKILL, &shutdown_pagekite);
  signal(SIGPIPE, SIG_IGN);
#endif

  init_kites += (argc-1-gotargs)/3;
  if (init_conns < init_kites) init_conns = 50 + 4 * init_kites;
  if (NULL == (m = pagekite_init("pagekiter",
                                 init_kites, init_kites, init_conns,
                                 NULL,
                                 flags,
                                 verbosity)))
  {
    pagekite_perror(m, argv[0]);
    safe_exit(EXIT_ERR_MANAGER_INIT);
  }

  /* Set all the parameters */
  pagekite_enable_watchdog(m, use_watchdog);
  pagekite_set_bail_on_errors(m, bail_on_errors);
  pagekite_set_conn_eviction_idle_s(m, conn_eviction_idle_s);
  pagekite_enable_lua_plugins(m, !lua_no_defaults, lua_settings);

  for (ac = gotargs; ac+3 < argc; ac += 3) {
    proto = argv[ac];
    kitename = argv[ac+1];
    secret = argv[ac+2];
    if (0 > pagekite_add_kite(m, proto, kitename, 0, secret, NULL, -1))
    {
      pagekite_perror(m, argv[0]);
      safe_exit(EXIT_ERR_ADD_KITE);
    }
  }

  for (ac = 0; ac < listen_portc; ac++) {
    if (0 > pagekite_add_relay_listener(m, listen_ports[ac], flags)) {
      pagekite_perror(m, argv[0]);
      safe_exit(EXIT_ERR_LISTEN);
    }
  }

  if (0 > pagekite_thread_start(m)) {
    pagekite_perror(m, argv[0]);
    safe_exit(EXIT_ERR_START_THREAD);
  }

  pagekite_thread_wait(m);
  pagekite_free(m);

  return 0;
}
