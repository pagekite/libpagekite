/******************************************************************************
pagekitec.c - A high-performance PageKite connector in C.

Usage: pagekitec [options] LPORT PROTO NAME.pagekite.me PPORT SECRET ...

*******************************************************************************

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

/* FIXME! */
#define HAVE_IPV6 1

#include <pagekite.h>
#include <pkcommon.h>

#include <unistd.h>
#include <sys/types.h>

#define EXIT_ERR_MANAGER_INIT 1
#define EXIT_ERR_USAGE 2
#define EXIT_ERR_ADD_KITE 3
#define EXIT_ERR_FRONTENDS 4
#define EXIT_ERR_START_THREAD 5
#define EXIT_ERR_ADD_LISTENER 6
#define EXIT_ERR_STATUS_FILE 7
#define MAX_PLUGIN_ARGS 128

/* When the app is idle and status is unchanged, this is the maximum
 * frequency for (minimum interval between) "no-op" changes to the status
 * summary file (-Y). Actual update frequency is driven by libpagekite's
 * internal housekeeping timers. The value is seconds. */
#define STATUS_MIN_INTERVAL 240

/* Enable this format using -Y json:/path/to/file/ */
#define STATUS_FORMAT_JSON (\
        "{\n" \
        "  \"pagekitec_version\": \"%s\",\n" \
        "  \"pagekitec_status\": \"%s\",\n"  \
        "  \"pagekitec_status_code\": %d,\n" \
        "  \"pagekitec_pid\": %d,\n"         \
        "  \"pagekitec_update_min_interval\": %d,\n" \
        "  \"pagekitec_update_ts\": %ld\n"   \
        "}\n")

/* Enable this format using -Y text:/path/to/file/ */
#define STATUS_FORMAT_TEXT (\
        "pagekitec_version: %s\n" \
        "pagekitec_status: %s\n"  \
        "pagekitec_status_code: %d\n" \
        "pagekitec_pid: %d\n"         \
        "pagekitec_update_min_interval: %d\n" \
        "pagekitec_update_ts: %ld\n")


pagekite_mgr m;


void usage(int ecode, char* message) {
  fprintf(stderr, "This is pagekitec.c from libpagekite %s.\n\n", PK_VERSION);
  fprintf(stderr, "Usage:\tpagekitec [options] LPORT PROTO"
                  " NAME.pagekite.me PPORT SECRET ...\n"
                  "Options:\n"
                  "\t-q\tDecrease verbosity (less log output)\n"
                  "\t-v\tIncrease verbosity (more log output)\n"
                  "\t-a x\tSet app name to x for logging\n"
                  "\t-s\tLog to syslog instead of to stderr\n"
                  "\t-Y f\tPeriodically summarize status to file f (JSON)\n");
#ifdef HAVE_OPENSSL
  fprintf(stderr, "\t-I\tConnect insecurely, without SSL.\n");
#endif
  fprintf(stderr, "\t-S\tStatic setup, disable FE failover and DDNS updates\n"
                  "\t-w D\tWhite-label configuration using domain D.\n"
                  "\t-r U\tSet a custom rejection URL, U.\n"
                  "\t-l D\tConnect to host D instead of localhost\n"
                  "\t-c N\tSet max connection count to N (default = 25)\n"
                  "\t-n N\tAlways connect to N spare frontends (default = 0)\n"
                  "\t-B N\tBail out (abort) after N logged errors\n"
                  "\t-E N\tAllow eviction of streams idle for >N seconds\n"
                  "\t-F x\tUse x (a DNS name) as frontend pool\n"
                  "\t-P x\tUse x (a port number) as frontend port\n"
                  "\t-R\tChoose frontends at random, instead of pinging\n"
                  "\t-N\tDisable DNS-based updates of available frontends\n"
                  "\t-V 46\tRequest DNS visibility on IPv6 and/or IPv4\n"
                  "\t-4\tDisable IPv4 frontends\n");
#ifdef HAVE_IPV6
  fprintf(stderr, "\t-6\tDisable IPv6 frontends\n");
#endif
  fprintf(stderr, "\t-C\tDisable auto-adding current DNS IP as a front-end\n"
                  "\t-W\tEnable watchdog thread (dumps core if we lock up)\n"
                  "\t-H\tDo not add X-Forwarded-Proto and X-Forwarded-For\n\n");
  if (message) fprintf(stderr, "\nError: %s\n", message);
  exit(ecode);
}

void raise_log_level(int sig) {
  if (sig) pagekite_set_log_mask(NULL, PK_LOG_ALL);
}

void dump_state_to_log(int sig) {
  if (sig) pagekite_dump_state_to_log(m);
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

void summarize_status(FILE* fd, char* format, char *status_msg, int status) {
  static time_t last_update = 0;
  static int last_status = 0;
  if (fd) {
    time_t now = time(0);
    if ((now - last_update > STATUS_MIN_INTERVAL) || (last_status != status)) {
      fseek(fd, 0, SEEK_SET);
      switch (status) {
        case PK_STATUS_STARTUP:      status_msg = "startup"; break;
        case PK_STATUS_CONNECTING:   status_msg = "connecting"; break;
        case PK_STATUS_UPDATING_DNS: status_msg = "updating_dns"; break;
        case PK_STATUS_FLYING:       status_msg = "flying"; break;
        case PK_STATUS_PROBLEMS:     status_msg = "problems"; break;
        case PK_STATUS_REJECTED:     status_msg = "rejected"; break;
        case PK_STATUS_NO_NETWORK:   status_msg = "no_network"; break;
      }

      fprintf(fd, format,
        PK_VERSION, status_msg, status, getpid(), STATUS_MIN_INTERVAL, now);

      if (0 == ftruncate(fileno(fd), ftell(fd))) fflush(fd);

      last_update = now;
      last_status = status;
    }
  }
}

int main(int argc, char **argv) {
  unsigned int bail_on_errors = 0;
  unsigned int conn_eviction_idle_s = 0;
  char* proto;
  char* kitename;
  char* secret;
  char* whitelabel_tld = NULL;
  char* app_name = "pagekitec";
  char* localhost = "localhost";
  char* rejection_url = NULL;
  char* status_summary_path = NULL;
  char* status_summary_fmt = NULL;
  FILE* status_summary_fd = NULL;
  int gotargs = 0;
  int verbosity = 0;
  int use_current = 1;
  int use_fake_ping = 0;
  int use_watchdog = 0;
  int modify_http_headers = 1;
  int max_conns = 25;
  int spare_frontends = 0;
  char* fe_hostname = NULL;
  int fe_port = 443;
  char* ddns_url = PAGEKITE_NET_DDNS;
  int ac;
  int pport;
  int lport;
  int flags = (PK_WITH_SSL
              |PK_WITH_IPV4
              |PK_WITH_DYNAMIC_FE_LIST
              |PK_WITH_SRAND_RESEED
              |PK_WITHOUT_SERVICE_FRONTENDS);
#ifdef HAVE_IPV6
  flags |= PK_WITH_IPV6;
#endif

  while (-1 != (ac = getopt(argc, argv,
                            "46a:B:c:CE:F:P:HIl:Nn:qr:RsSvV:Ww:Y:Z"))) {
    switch (ac) {
      case '4':
        flags &= ~PK_WITH_IPV4;
        break;
#ifdef HAVE_IPV6
      case '6':
        flags &= ~PK_WITH_IPV6;
        break;
#endif
      case 'V':
        gotargs++;
        if (strchr(optarg, '4') != NULL) flags |= PK_WITH_IPV4_DNS;
        if (strchr(optarg, '6') != NULL) flags |= PK_WITH_IPV6_DNS;
        break;
      case 'a':
        gotargs++;
        app_name = strdup(optarg);
        break;
      case 's':
        flags |= PK_WITH_SYSLOG;
        break;
      case 'Y':
        gotargs++;
        status_summary_path = strdup(optarg);
        if (strncasecmp(status_summary_path, "json:", 5) == 0) {
          status_summary_fmt = STATUS_FORMAT_JSON;
          status_summary_path += 5;
        }
        else if (strncasecmp(status_summary_path, "text:", 5) == 0) {
          status_summary_fmt = STATUS_FORMAT_TEXT;
          status_summary_path += 5;
        }
        else {
          status_summary_fmt = STATUS_FORMAT_JSON;
        }
        status_summary_fd = fopen(status_summary_path, "w+");
        if (!status_summary_fd) usage(EXIT_ERR_STATUS_FILE, strerror(errno));
        summarize_status(status_summary_fd, status_summary_fmt, "startup", 0);
        break;
      case 'N':
        flags &= ~PK_WITH_DYNAMIC_FE_LIST;
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
        flags &= ~PK_WITH_SSL;
        break;
      case 'r':
        gotargs++;
        rejection_url = strdup(optarg);
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
      case 'H':
        modify_http_headers = 0;
        break;
      case 'w':
        gotargs++;
        whitelabel_tld = strdup(optarg);
        break;
      case 'l':
        gotargs++;
        localhost = strdup(optarg);
        break;
      case 'F':
        gotargs++;
        assert(fe_hostname == NULL);
        fe_hostname = strdup(optarg);
        break;
      case 'P':
        gotargs++;
        if (1 == sscanf(optarg, "%d", &fe_port)) break;
        usage(EXIT_ERR_USAGE, "Invalid argument to -P");
      case 'B':
        gotargs++;
        if (1 == sscanf(optarg, "%u", &bail_on_errors)) break;
        usage(EXIT_ERR_USAGE, "Invalid argument to -B");
      case 'c':
        gotargs++;
        if (1 == sscanf(optarg, "%d", &max_conns)) break;
        usage(EXIT_ERR_USAGE, "Invalid argument to -c");
      case 'E':
        gotargs++;
        if (1 == sscanf(optarg, "%u", &conn_eviction_idle_s)) break;
        usage(EXIT_ERR_USAGE, "Invalid argument to -E");
      case 'n':
        gotargs++;
        if (1 == sscanf(optarg, "%d", &spare_frontends)) break;
        usage(EXIT_ERR_USAGE, "Invalid argument to -n");
      default:
        usage(EXIT_ERR_USAGE, "Unrecognized option");
    }
    gotargs++;
  }

  if ((argc-1-gotargs) < 5 || ((argc-1-gotargs) % 5) != 0) {
    usage(EXIT_ERR_USAGE,
          "Invalid kite specification (wrong number of arguments)");
  }

#ifndef _MSC_VER
  signal(SIGUSR1, &raise_log_level);
  signal(SIGUSR2, &dump_state_to_log);
  signal(SIGQUIT, &shutdown_pagekite);
  signal(SIGINT, &shutdown_pagekite);
  signal(SIGPIPE, SIG_IGN);
#endif

  if (whitelabel_tld != NULL)
  {
    if (NULL == (m = pagekite_init_whitelabel(
      app_name,
      1 + (argc-1-gotargs)/5, /* Kites */
      max_conns,
      flags,
      verbosity,
      whitelabel_tld)))
    {
      pagekite_perror(m, argv[0]);
      safe_exit(EXIT_ERR_MANAGER_INIT);
    }
  }
  else if (NULL == (m = pagekite_init(
    app_name,
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
  pagekite_enable_http_forwarding_headers(m, modify_http_headers);
  pagekite_enable_fake_ping(m, use_fake_ping);
  pagekite_set_bail_on_errors(m, bail_on_errors);
  pagekite_set_conn_eviction_idle_s(m, conn_eviction_idle_s);
  if (rejection_url != NULL) pagekite_set_rejection_url(m, rejection_url);

  /* Move the logging to the event API, mostly for testing. */
  if ((verbosity < 4) && (0 == (flags & PK_WITH_SYSLOG))) {
    pagekite_set_log_destination(m, PK_LOG_DEST_NONE);
    pagekite_set_event_mask(m, PK_EV_MASK_LOGGING | PK_EV_MASK_MISC);
  }
  else {
    pagekite_set_event_mask(m, PK_EV_MASK_MISC);
  }

  for (ac = gotargs; ac+5 < argc; ac += 5) {
    if ((1 != sscanf(argv[ac+1], "%d", &lport)) ||
        (1 != sscanf(argv[ac+4], "%d", &pport))) {
      pagekite_free(m);
      usage(EXIT_ERR_USAGE, "Failed to parse kite specification port numbers");
    }
    proto = argv[ac+2];
    kitename = argv[ac+3];
    secret = argv[ac+5];
    if ((0 > pagekite_add_kite(m, proto, kitename, pport, secret,
                               localhost, lport)) ||
        (use_current && (0 > (pagekite_add_frontend(m, kitename, pport)))))
    {
      pagekite_perror(m, argv[0]);
      pagekite_free(m);
      safe_exit(EXIT_ERR_ADD_KITE);
    }
  }

  /* The API could do this stuff on INIT, but since we allow for manually
     specifying a front-end hostname, we do things by hand. */
  if (fe_hostname) {
    if (0 > pagekite_lookup_and_add_frontend(m, fe_hostname, fe_port,
                                             flags & PK_WITH_DYNAMIC_FE_LIST))
    {
      pagekite_perror(m, argv[0]);
      pagekite_free(m);
      safe_exit(EXIT_ERR_FRONTENDS);
    }
  }
  else if (whitelabel_tld != NULL) {
    if (0 > pagekite_add_whitelabel_frontends(m, flags, whitelabel_tld)) {
      pagekite_perror(m, argv[0]);
      pagekite_free(m);
      safe_exit(EXIT_ERR_FRONTENDS);
    }
  }
  else if (ddns_url != NULL) {
    if (0 > pagekite_add_service_frontends(m, flags)) {
      pagekite_perror(m, argv[0]);
      pagekite_free(m);
      safe_exit(EXIT_ERR_FRONTENDS);
    }
  }

  if (0 > pagekite_thread_start(m)) {
    pagekite_perror(m, argv[0]);
    pagekite_free(m);
    safe_exit(EXIT_ERR_START_THREAD);
  }
  summarize_status(
    status_summary_fd, status_summary_fmt, "unknown", pagekite_get_status(m));

  unsigned int eid;
  while (PK_EV_SHUTDOWN != (
    (eid = pagekite_await_event(m, 1)) & PK_EV_TYPE_MASK))
  {
    switch (eid & PK_EV_TYPE_MASK) {
      case PK_EV_LOGGING:
        fprintf(stderr, "%s\n", pagekite_get_event_str(m, eid));
        break;
      default:
        if (verbosity > 2) fprintf(stderr, "[Got event 0x%8.8x]\n", eid);
    }
    pagekite_event_respond(m, eid, PK_EV_RESPOND_DEFAULT);
    summarize_status(
      status_summary_fd, status_summary_fmt, "unknown", pagekite_get_status(m));
  }
  pagekite_thread_wait(m);
  pagekite_free(m);

  summarize_status(status_summary_fd, status_summary_fmt, "shutdown", 0);
  return 0;
}
