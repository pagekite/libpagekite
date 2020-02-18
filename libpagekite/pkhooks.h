/******************************************************************************
pkhooks.h - Callbacks for various internal events

This file is Copyright 2011-2020, The Beanstalks Project ehf.

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

typedef enum {
  PK_HOOK_STOPPED         =  0, /* 0, pk_manager, NULL           => (ignored) */
  PK_HOOK_START_EV_LOOP   =  1, /* 0, pk_manager, NULL           => ev_loop? */
  PK_HOOK_START_BLOCKER   =  2, /* 0, pk_blocker, pk_manager     => (ignored) */

  PK_HOOK_LOG             =  6, /* bytes, log_line, NULL         => log? */
  PK_HOOK_TICK            =  7, /* pk_time(), pk_manager, NULL   => (ignored) */
  PK_HOOK_CHECK_WORLD     =  8, /* 0=begin, pk_blocker, pk_manager => check? */
                                /* 1=end, pk_blocker, pk_manager => (ignored) */
  PK_HOOK_CHECK_TUNNELS   =  9, /* 0=begin, pk_blocker, pk_manager => check? */
                                /* 1=end, pk_blocker, pk_manager => (ignored) */
  PK_HOOK_STATE_CHANGED   = 10, /* 0, NULL, NULL                 => (ignored) */

  PK_HOOK_BE_CONN_WRAP    = 14,
  PK_HOOK_FE_CONN_WRAP    = 15,
  PK_HOOK_BE_CONN_OPENED  = 16,
  PK_HOOK_BE_CONN_CLOSED  = 17,
  PK_HOOK_FE_CONN_OPENED  = 18,
  PK_HOOK_FE_CONN_CLOSED  = 19,
  PK_HOOK_FE_DISCONNECT   = 20,

  PK_HOOK_CHUNK_INCOMING  = 28, /* 0, pk_chunk, pk_backend_conn => (ignored) */
  PK_HOOK_CHUNK_OUTGOING  = 29,
  PK_HOOK_DATA_INCOMING   = 30,
  PK_HOOK_DATA_OUTGOING   = 31, /* 0, pk_chunk, pk_backend_conn => pkc_write? */
} pk_hook_t;
#define PK_HOOK_MAX         32

#ifndef __IN_PKHOOKS_C__
extern pagekite_callback2_t* pk_hooks[];
#else
pagekite_callback2_t* pk_hooks[PK_HOOK_MAX] = {
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};
#endif

/* This macro will return -1 (a true value) if the hook is undefined,
 * or the return value of the callback function itself. */
#define PK_HOOK(n, i, p1, p2) \
        ((pk_hooks[n] == NULL) ? -1 : (pk_hooks[n])(n, i, p1, p2))


struct pke_event {
  time_t          posted;
  unsigned int    event_code;
  int             event_int;
  char*           event_str;
  unsigned int    response_code;
  int*            response_int;
  char**          response_str;
  pthread_cond_t  trigger;
};

struct pke_events {
  struct pke_event*   events;
  unsigned int        event_mask;
  unsigned int        event_max;
  unsigned int        event_ptr;
  pthread_mutex_t     lock;
  pthread_cond_t      trigger;
};


void pke_init_events             (struct pke_events*, unsigned int);
void pke_free_event              (struct pke_events*, unsigned int);
void pke_post_event              (struct pke_events*, unsigned int,
                                  int, const char*);
struct pke_event* pke_post_blocking_event(
                                  struct pke_events*, unsigned int,
                                  int, const char*, int*, char**);
struct pke_event* pke_await_event(struct pke_events*, int);
struct pke_event* pke_get_event  (struct pke_events*, unsigned int);
void pke_post_response           (struct pke_events*, unsigned int,
                                  unsigned int, int, const char*);
void pke_cancel_all_events       (struct pke_events*);
