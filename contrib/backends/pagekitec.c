/******************************************************************************
pagekitec.c - A high-performance PageKite connector in C.

Usage: pagekitec [options] LPORT PROTO NAME.pagekite.me PPORT SECRET ...

*******************************************************************************

This file is Copyright 2011-2015, The Beanstalks Project ehf.

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

#include <pagekite.h>
#include "common.h"


#define EXIT_ERR_MANAGER_INIT 1
#define EXIT_ERR_USAGE 2
#define EXIT_ERR_ADD_KITE 3
#define EXIT_ERR_FRONTENDS 4
#define EXIT_ERR_START_THREAD 5


void usage(int ecode) {
  fprintf(stderr, "This is pagekitec.c from libpagekite %s.\n\n", PK_VERSION);
  fprintf(stderr, "Usage:\tpagekitec [options] LPORT PROTO"
                  " NAME.pagekite.me PPORT SECRET ...\n"
                  "Options:\n"
                  "\t-q\tDecrease verbosity (less log output)\n"
                  "\t-v\tIncrease verbosity (more log output)\n");
#ifdef HAVE_OPENSSL
  fprintf(stderr, "\t-I\tConnect insecurely, without SSL.\n");
#endif
  fprintf(stderr, "\t-S\tStatic setup, disable FE failover and DDNS updates\n"
                  "\t-c N\tSet max connection count to N (default = 25)\n"
                  "\t-n N\tAlways connect to N spare frontends (default = 0)\n"
                  "\t-B N\tBail out (abort) after N logged errors\n"
                  "\t-E N\tAllow eviction of streams idle for >N seconds\n"
                  "\t-F x\tUse x (a DNS name) as frontend pool\n"
                  "\t-R\tChoose frontends at random, instead of pinging\n"
                  "\t-4\tDisable IPv4 frontends\n");
#ifdef HAVE_IPV6
  fprintf(stderr, "\t-6\tDisable IPv6 frontends\n");
#endif
  fprintf(stderr, "\t-C\tDisable auto-adding current DNS IP as a front-end\n"
                  "\t-W\tEnable watchdog thread (dumps core if we lock up)\n"
                  "\n");
  exit(ecode);
}

void raise_log_level(int sig) {
  if (sig) pagekite_set_log_mask(NULL, PK_LOG_ALL);
}

void safe_exit(int code) {
#ifdef _MSC_VER
  fprintf(stderr, "Exiting with status code %d.\n", code);
#endif
  fclose(stderr);
  exit(code);
}

int main(int argc, char **argv) {
  pagekite_mgr m;
  unsigned int bail_on_errors = 0;
  unsigned int conn_eviction_idle_s = 0;
  char* proto;
  char* kitename;
  char* secret;
  int gotargs = 0;
  int verbosity = 0;
  int use_ipv4 = 1;
#ifdef HAVE_IPV6
  int use_ipv6 = 1;
#endif
  int use_current = 1;
  int use_ssl = 1;
  int use_fake_ping = 0;
  int use_watchdog = 0;
  int max_conns = 25;
  int spare_frontends = 0;
  char* fe_hostname = NULL;
  char* ddns_url = PAGEKITE_NET_DDNS;
  int ac;
  int pport;
  int lport;

  /* FIXME: Is this too lame? */
  srand(time(0) ^ getpid());

  while (-1 != (ac = getopt(argc, argv, "46c:B:CE:F:In:qRSvWZ"))) {
    switch (ac) {
      case '4':
        use_ipv4 = 0;
        break;
      case '6':
#ifdef HAVE_IPV6
        use_ipv6 = 0;
#endif
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
        use_fake_ping = 1;
        break;
      case 'S':
        ddns_url = NULL;
        break;
      case 'W':
        use_watchdog = 1;
        break;
      case 'F':
        gotargs++;
        assert(fe_hostname == NULL);
        fe_hostname = strdup(optarg);
        break;
      case 'B':
        gotargs++;
        if (1 == sscanf(optarg, "%u", &bail_on_errors)) break;
        usage(EXIT_ERR_USAGE);
      case 'c':
        gotargs++;
        if (1 == sscanf(optarg, "%d", &max_conns)) break;
        usage(EXIT_ERR_USAGE);
      case 'E':
        gotargs++;
        if (1 == sscanf(optarg, "%u", &conn_eviction_idle_s)) break;
        usage(EXIT_ERR_USAGE);
      case 'n':
        gotargs++;
        if (1 == sscanf(optarg, "%d", &spare_frontends)) break;
        usage(EXIT_ERR_USAGE);
      default:
        usage(EXIT_ERR_USAGE);
    }
    gotargs++;
  }

  if ((argc-1-gotargs) < 5 || ((argc-1-gotargs) % 5) != 0) {
    usage(EXIT_ERR_USAGE);
  }

#ifndef _MSC_VER
  signal(SIGUSR1, &raise_log_level);
#endif

  int flags = 0;
  if (use_ssl) flags |= PK_WITH_SSL;
  if (use_ipv4) flags |= PK_WITH_IPV4;
#ifdef HAVE_IPV6
  if (use_ipv6) flags |= PK_WITH_IPV6;
#endif

  if (NULL == (m = pagekite_init("pagekitec",
                                 1 + (argc-1-gotargs)/5, /* Kites */
                                 PAGEKITE_NET_FE_MAX,
                                 max_conns,
                                 ddns_url,
                                 flags,
                                 verbosity)))
  {
    pagekite_perror(m, argv[0]);
    safe_exit(EXIT_ERR_MANAGER_INIT);
  }

  /* Set all the parameters */
  pagekite_want_spare_frontends(m, spare_frontends);
  pagekite_enable_watchdog(m, use_watchdog);
  pagekite_enable_fake_ping(m, use_fake_ping);
  pagekite_set_bail_on_errors(m, bail_on_errors);
  pagekite_set_conn_eviction_idle_s(m, conn_eviction_idle_s);

  for (ac = gotargs; ac+5 < argc; ac += 5) {
    if ((1 != sscanf(argv[ac+1], "%d", &lport)) ||
        (1 != sscanf(argv[ac+4], "%d", &pport))) {
      usage(EXIT_ERR_USAGE);
    }
    proto = argv[ac+2];
    kitename = argv[ac+3];
    secret = argv[ac+5];
    if ((0 > pagekite_add_kite(m, proto, kitename, 0, secret,
                               "localhost", lport)) ||
        (use_current && (0 > (pagekite_add_frontend(m, kitename, pport)))))
    {
      pagekite_perror(m, argv[0]);
      safe_exit(EXIT_ERR_ADD_KITE);
    }
  }

  /* The API could do this stuff on INIT, but since we allow for manually
     specifying a front-end hostname, we do things by hand. */
  if (fe_hostname) {
    if (0 > pagekite_add_frontend(m, fe_hostname, 443)) {
      pagekite_perror(m, argv[0]);
      safe_exit(EXIT_ERR_FRONTENDS);
    }
  }
  else if (ddns_url != NULL) {
    if (0 > pagekite_add_service_frontends(m, flags)) {
      pagekite_perror(m, argv[0]);
      safe_exit(EXIT_ERR_FRONTENDS);
    }
  }

  if (0 > pagekite_start(m)) {
    pagekite_perror(m, argv[0]);
    safe_exit(EXIT_ERR_START_THREAD);
  }

  pagekite_wait(m);
  pagekite_free(m);

  return 0;
}
