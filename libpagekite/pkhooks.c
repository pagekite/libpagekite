/******************************************************************************
pkhooks.c - Callbacks for various internal events

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

*******************************************************************************

HOW IT WORKS:

See also: doc/Event_API.md

We use a queue of events as a coordination point between threads, so
consumers of the API don't need to understand our internal threading and so
we don't need to care how many threads are active on the consumer's side.

The events are identified using basic primitives (integers and strings) to
facilitate cross-platform API compatibility.

So a consumer of the API just needs to follow this pattern:

  int timeout = 5;                     // Seconds
  pagekite_set_event_mask(PK_EV_ALL);  // Or maybe not all...
  unsigned int event_code;
  while (PK_EV_SHUTDOWN != (
    (event_code = pagekite_await_event(pkm, timeout)) & PK_EV_TYPE_MASK))
  {
    switch (event_code & PK_EV_TYPE_MASK) {

      case PK_EV_FANCYCODE:
        pagekite_event_respond_with_data(pkm, event_code,
                                         PK_EV_RESPOND_AWESOME,
                                         reponse_integer, response_string);

      ...

      default:
        pagekite_event_respond(pkm, event_code, PK_EV_RESPOND_DEFAULT);
    }
  }

Suppressing certain classes of events using the event mask will squelch
the events at their "source", thus reducing lock contention and overall
work.

Higher level language bindings can provide a nicer/more idiomatic interface.

Note that this file contains the lower-level libpagekite internal API, the
outward facing public API methods described above are defined in pagekite.c.

******************************************************************************/
#define __IN_PKHOOKS_C__

#define PAGEKITE_CONSTANTS_ONLY
#include "pagekite.h"

#include "pkcommon.h"
#include "pkutils.h"
#include "pkerror.h"
#include "pkconn.h"
#include "pkstate.h"
#include "pkhooks.h"
#include "pkutils.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"

#define _EV(pke, event_code) \
  (pke->events + ((event_code & PK_EV_SLOT_MASK) >> PK_EV_SLOT_SHIFT))

struct pke_events* _pke_default_pke = NULL;


void pke_init_events(struct pke_events* pke, unsigned int threads) {
  unsigned int bytes = 0;
  static pthread_condattr_t shared_condattr;
  pke->event_mask = PK_EV_NONE;
  pke->event_ptr = 0;
  pke->event_max = threads * 12;
  if (pke->event_max > PK_EV_SLOTS_MAX) pke->event_max = PK_EV_SLOTS_MAX;

  PK_TRACE_FUNCTION;

  /* Make sure our condition variables are configured to use the same
   * clock as pk_gettime(). */
  pthread_condattr_init(&shared_condattr);
  pk_pthread_condattr_setclock(&shared_condattr);

  bytes = sizeof(struct pke_event) * pke->event_max;
  pke->events = malloc(bytes);
  memset(pke->events, 0, bytes);

  for (int i = 0; i < pke->event_max; i++) {
    pke->events[i].event_code = (i << PK_EV_SLOT_SHIFT);
    pthread_cond_init(&(pke->events[i].trigger), &shared_condattr);
  }

  /* Event 0 is reserved for the NONE event. */
  pke->events[0].event_code = PK_EV_NONE;

  pthread_mutex_init(&(pke->lock), NULL);
  pthread_cond_init(&(pke->trigger), &shared_condattr);

  _pke_default_pke = pke;
}

void _pke_unlocked_free_event(struct pke_events* pke, unsigned int event_code)
{
  PK_TRACE_FUNCTION;

  struct pke_event* ev = _EV(pke, event_code);
  ev->event_code &= PK_EV_SLOT_MASK;
  if (ev->event_str != NULL) free(ev->event_str);
  ev->event_str = NULL;
  ev->event_int = 0;
  ev->response_code = 0;
  ev->response_str = NULL;
  ev->response_int = NULL;
  ev->posted = 0;
}

struct pke_event* _pke_get_oldest_event(struct pke_events* pke,
                                        int nonzero, int excl_id)
{
  PK_TRACE_FUNCTION;

  struct pke_event* oldest = NULL;
  struct pke_event* ev = &(pke->events[1]);
  for (int i = 1; i < pke->event_max; i++, ev++) {
    if ((0 == (ev->event_code & excl_id)) &&
        ((oldest == NULL) || (ev->posted < oldest->posted)) &&
        ((0 == nonzero) || (ev->posted > 0)))
    {
      oldest = ev;
      if (oldest->posted == 0) return oldest;
    }
  }
  return oldest;
}

struct pke_event* _pke_unlocked_post_event(
  struct pke_events* pke,
  int event_type, int event_int, const char* event_str,
  int* response_int, char** response_str)
{
  PK_TRACE_FUNCTION;

  if ((pke->event_mask != PK_EV_ALL) &&
      (0 == (event_type & pke->event_mask))) return NULL;

  struct pke_event* ev = _pke_get_oldest_event(
    pke, 0, PK_EV_IS_BLOCKING | PK_EV_PROCESSING);
  if (ev == NULL) ev = &(pke->events[1]);

  ev->event_code = (ev->event_code & PK_EV_SLOT_MASK) | event_type;
  ev->posted = pk_time();
  if (ev->event_str != NULL) free(ev->event_str);
  if (event_str != NULL) {
    ev->event_str = strdup(event_str);
  }
  else {
    ev->event_str = NULL;
  }
  ev->event_int = event_int;
  ev->response_code = 0;
  ev->response_int = response_int;
  ev->response_str = response_str;
  return ev;
}

void pke_free_event(struct pke_events* pke, unsigned int event_code)
{
  PK_TRACE_FUNCTION;

  pke = (pke != NULL) ? pke : _pke_default_pke;
  if (pke == NULL) return;

  pthread_mutex_lock(&(pke->lock));
  _pke_unlocked_free_event(pke, event_code);
  pthread_mutex_unlock(&(pke->lock));
}

/* Fire and forget: post a non-blocking event.
 * Provided data, if any, will be copied.
 */
void pke_post_event(
  struct pke_events* pke,
  unsigned int event_type, int event_int, const char* event_str)
{
  PK_TRACE_FUNCTION;

  pke = (pke != NULL) ? pke : _pke_default_pke;
  if (pke == NULL) return;

  if ((pke->event_mask != PK_EV_ALL) &&
      (0 == (event_type & pke->event_mask))) return;

  pthread_mutex_lock(&(pke->lock));
  _pke_unlocked_post_event(pke, event_type, event_int, event_str, NULL, NULL);
  if (PK_EV_SHUTDOWN == (event_type & PK_EV_TYPE_MASK)) {
    pthread_cond_broadcast(&(pke->trigger));
  }
  else {
    pthread_cond_signal(&(pke->trigger));
  }
  pthread_mutex_unlock(&(pke->lock));
}

/* Synchronization: post a blocking event.
 *
 * The function will block until a response has been posted.
 *
 * Afterwards, pke_free_event(...) MUST be called to clean up.
 *
 * If response_int and response_str are provided, they MAY be populated
 * with an integer value and a pointer to a freshly malloc'ed response
 * string. It is the caller's responsibility to free the memory when the
 * data has been processed.
 */
struct pke_event* pke_post_blocking_event(
  struct pke_events* pke,
  unsigned int event_type, int event_int, const char* event_str,
  int* response_int, char** response_str)
{
  PK_TRACE_FUNCTION;

  if (response_int != NULL) *response_int = 0;
  if (response_str != NULL) *response_str = NULL;

  pke = (pke != NULL) ? pke : _pke_default_pke;
  if (pke == NULL) return PK_EV_RESPOND_DEFAULT;

  if ((pke->event_mask != PK_EV_ALL) &&
      (0 == (event_type & pke->event_mask))) return NULL;

  pthread_mutex_lock(&(pke->lock));

  struct pke_event* ev = _pke_unlocked_post_event(
    pke,
    event_type | PK_EV_IS_BLOCKING, event_int, event_str,
    response_int, response_str);

  if (PK_EV_SHUTDOWN == (event_type & PK_EV_TYPE_MASK)) {
    pthread_cond_broadcast(&(pke->trigger));
  }
  else {
    pthread_cond_signal(&(pke->trigger));
  }
  pthread_cond_wait(&(ev->trigger), &(pke->lock));
  pthread_mutex_unlock(&(pke->lock));

  return ev;
}

struct pke_event* pke_await_event(struct pke_events* pke, int timeout)
{
  PK_TRACE_FUNCTION;

  pke = (pke != NULL) ? pke : _pke_default_pke;
  struct pke_event* oldest = NULL;

  struct timespec deadline;
  pk_gettime(&deadline);
  deadline.tv_sec += timeout;

  do {
    /* Search for an event... */
    pthread_mutex_lock(&(pke->lock));

    oldest = _pke_get_oldest_event(pke, 1, PK_EV_PROCESSING);
    if ((oldest != NULL) && (0 < oldest->posted))
      oldest->event_code |= PK_EV_PROCESSING;

    pthread_mutex_unlock(&(pke->lock));

    if ((oldest != NULL) && (0 < oldest->posted)) return oldest;

    /* No event found, block ourselves until someone posts one */
    pthread_mutex_lock(&(pke->lock));
    while (0 != pthread_cond_timedwait(&(pke->trigger), &(pke->lock),
                                       &deadline)) {
      pthread_mutex_unlock(&(pke->lock));
      return &(pke->events[0]);
    }
    pthread_mutex_unlock(&(pke->lock));

  } while (1);
}

struct pke_event* pke_get_event(struct pke_events* pke, unsigned int event_code) {
  pke = (pke != NULL) ? pke : _pke_default_pke;
  return _EV(pke, event_code);
}

void pke_post_response(struct pke_events* pke, unsigned int event_code,
                       unsigned int rcode,
                       int response_int, const char* response_str) {
  PK_TRACE_FUNCTION;

  pke = (pke != NULL) ? pke : _pke_default_pke;

  struct pke_event* ev = _EV(pke, event_code);
  if (ev->event_code == PK_EV_NONE) return;
  assert(0 != (ev->event_code & PK_EV_PROCESSING));

  /* Shutdown events just stay pinned and keep getting served. */
  if (PK_EV_SHUTDOWN == (ev->event_code & PK_EV_TYPE_MASK)) return;

  if (0 == (ev->event_code & PK_EV_IS_BLOCKING)) {
    /* Non-blocking events don't take responses, so we just mark the
     * event as processed and short-circuit. */
    ev->posted = 0;
    ev->event_code &= (0xffffffff ^ PK_EV_PROCESSING);
    return;
  }

  /* If response data is provided, and the event poster provided a
   * place for accepting responses, copy the data over. */
  if (NULL != ev->response_int) *(ev->response_int) = response_int;
  if (NULL != ev->response_str) {
    if (NULL != response_str) {
      *(ev->response_str) = strdup(response_str);
    }
    else {
      *(ev->response_str) = NULL;
    }
  }
  ev->response_code = rcode;

  /* Release the blocked thread that posted the event. */
  pthread_cond_signal(&(ev->trigger));
}

void pke_cancel_all_events(struct pke_events* pke) {
  PK_TRACE_FUNCTION;

  pke = (pke != NULL) ? pke : _pke_default_pke;

  struct pke_event* ev = &(pke->events[1]);
  for (int i = 1; i < pke->event_max; i++, ev++) {
    if (ev->posted && (0 == (ev->event_code & PK_EV_PROCESSING))) {
      ev->event_code |= PK_EV_PROCESSING;
      pke_post_response(pke, ev->event_code, PK_EV_RESPOND_DEFAULT, 0, NULL);
    }
  }

  pthread_cond_broadcast(&(pke->trigger));
}


/* ************************************************************************* */

void* pke_event_test_poster(void* void_pke) {
  struct pke_events* pke = (struct pke_events*) void_pke;
  struct pke_event* ev;
  int r_int;
  char* r_str;

  pke_post_event(NULL, 123, 0, NULL); fprintf(stderr, "."); sleep(1);
  pke_post_event(NULL, 345, 0, NULL); fprintf(stderr, "."); sleep(1);
  pke_post_event(NULL, 678, 0, NULL); fprintf(stderr, "."); sleep(1);
  pke_post_event(NULL, 901, 0, NULL); fprintf(stderr, "."); sleep(1);

  ev = pke_post_blocking_event(pke, 255, 9, "hello", &r_int, &r_str);
  assert(76 == ev->response_code);
  assert(r_int == 9);
  assert(r_str != NULL);
  assert(strcasecmp(r_str, "hello") == 0);
  free(r_str);
  pke_free_event(pke, ev->event_code);

  return void_pke;
}

int pke_events_test() {
  struct pke_events pke;
  struct pke_event* ev = NULL;
  pthread_t poster;

  pke_init_events(&pke, 100);
  pke.event_mask = PK_EV_ALL;
  assert(pke.event_max <= PK_EV_SLOTS_MAX);

  fprintf(stderr, "Testing events ");
  pthread_create(&poster, NULL, pke_event_test_poster, (void*) &pke);
  sleep(2);

  ev = pke_await_event(&pke, 10);
  fprintf(stderr, "+(%x)", ev->event_code);
  assert(123 == (ev->event_code & PK_EV_TYPE_MASK));
  pke_post_response(&pke, ev->event_code, PK_EV_RESPOND_DEFAULT, 0, NULL);

  ev = pke_await_event(&pke, 10);
  fprintf(stderr, "+(%x)", ev->event_code);
  assert(345 == (ev->event_code & PK_EV_TYPE_MASK));
  pke_post_response(&pke, ev->event_code, PK_EV_RESPOND_DEFAULT, 0, NULL);

  ev = pke_await_event(&pke, 10);
  fprintf(stderr, "+(%x)", ev->event_code);
  assert(678 == (ev->event_code & PK_EV_TYPE_MASK));
  pke_post_response(&pke, ev->event_code, PK_EV_RESPOND_DEFAULT, 0, NULL);

  ev = pke_await_event(&pke, 10);
  fprintf(stderr, "+(%x)", ev->event_code);
  assert(901 == (ev->event_code & PK_EV_TYPE_MASK));
  pke_post_response(&pke, ev->event_code, PK_EV_RESPOND_DEFAULT, 0, NULL);

  ev = pke_await_event(&pke, 10);
  fprintf(stderr, "+(%x)", ev->event_code);
  assert(255 == (ev->event_code & PK_EV_TYPE_MASK));
  assert(ev->event_int == 9);
  assert(strcasecmp(ev->event_str, "hello") == 0);
  pke_post_response(&pke, ev->event_code, 76, ev->event_int, ev->event_str);

  pthread_join(poster, NULL);
  fprintf(stderr, "\n");

  /* Avoid breaking subsequent tests. */
  _pke_default_pke = NULL;
  return 1;
}

