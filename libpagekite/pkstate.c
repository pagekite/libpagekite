/******************************************************************************
pkstate.c - Global program state.

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
#define __IN_PKSTATE_C__

#define PAGEKITE_CONSTANTS_ONLY
#include "pagekite.h"

#include "pkcommon.h"
#include "pkstate.h"


void pks_global_init(unsigned int log_mask) {
  memset(&pk_state, 0, sizeof(struct pk_global_state));
  pthread_mutex_init(&(pk_state.lock), NULL);
  pthread_cond_init(&(pk_state.cond), NULL);
  pk_state.log_ring_start = pk_state.log_ring_end = pk_state.log_ring_buffer;
  pk_state.log_file = stderr;
  pk_state.log_mask = log_mask;

  pk_state.dns_check_name = "google.com";  /* A safe default with a low TTL */
  pk_state.bail_on_errors = 0;
  pk_state.conn_eviction_idle_s = 0;
  pk_state.socket_timeout_s = PK_DEFAULT_SOCKET_TIMEOUT;
  pk_state.fake_ping = 0;
  pk_state.ssl_ciphers = PKS_DEFAULT_CIPHERS;
  pk_state.ssl_cert_names = NULL;
  pk_state.use_ipv4 = 1;
#ifdef HAVE_IPV6
  pk_state.use_ipv6 = 1;
#endif

  pk_state.have_ssl = 0;
  pk_state.app_id_long = "libpagekite";
  pk_state.app_id_short = PK_VERSION;

  pk_state.quota_days = -1;
  pk_state.quota_conns = -1;
  pk_state.quota_mb = -1;
}

#define WRAP(p) if (p >= pk_state.log_ring_buffer+PKS_LOG_DATA_MAX) \
                    p -= PKS_LOG_DATA_MAX;

int pks_logcopy(const char *src, size_t len)
{
  int tmp;

  len += 1; /* Make room for \n */
  if (len >= PKS_LOG_DATA_MAX) return -1;

  pthread_mutex_lock(&(pk_state.lock));

  /* Move the buffer beginning forward to make room... */
  if (((pk_state.log_ring_end < pk_state.log_ring_start) &&
       (pk_state.log_ring_end + len >= pk_state.log_ring_start)) ||
      ((pk_state.log_ring_end > pk_state.log_ring_start) &&
       (pk_state.log_ring_end + len >= pk_state.log_ring_start + PKS_LOG_DATA_MAX)))
  {
    pk_state.log_ring_start += len;
    WRAP(pk_state.log_ring_start);
    while ((*pk_state.log_ring_start != '\n') &&
           (pk_state.log_ring_start != pk_state.log_ring_end)) {
      pk_state.log_ring_start++;
      WRAP(pk_state.log_ring_start);
    }
    while (*pk_state.log_ring_start == '\n') {
      pk_state.log_ring_start++;
      WRAP(pk_state.log_ring_start);
    }
  }

  /* At this point we have room, we just need to figure out which kind of
   * copy this is, a wrapping or non-wrapping copy. */

  /* Stop counting the \n we're going to add manually. */
  len -= 1;

  /* [=>  s------e=] */
  if ((pk_state.log_ring_end > pk_state.log_ring_start) &&
      (pk_state.log_ring_end + len >= pk_state.log_ring_buffer + PKS_LOG_DATA_MAX))
  {
    tmp = PKS_LOG_DATA_MAX - (pk_state.log_ring_end - pk_state.log_ring_buffer);
    memcpy(pk_state.log_ring_end, src, tmp);
    memcpy(pk_state.log_ring_buffer, src + tmp, len - tmp);
    pk_state.log_ring_end = pk_state.log_ring_buffer + len - tmp;
  }
  /* [----e==>  s--] or [ s------e==> ] */
  else {
    memcpy(pk_state.log_ring_end, src, len);
    pk_state.log_ring_end += len;
  }

  WRAP(pk_state.log_ring_end); *pk_state.log_ring_end++ = '\n';
  WRAP(pk_state.log_ring_end); *pk_state.log_ring_end   = '\0';

  pthread_cond_broadcast(&(pk_state.cond));
  pthread_mutex_unlock(&(pk_state.lock));
  return len;
}

/* WARNING: dest must be of size PKS_LOG_DATA_MAX or bigger */
void pks_copylog(char *dest)
{
  pthread_mutex_lock(&(pk_state.lock));
  strcpy(dest, pk_state.log_ring_start);
  if (pk_state.log_ring_end < pk_state.log_ring_start)
    strcat(dest, pk_state.log_ring_buffer);
  pthread_mutex_unlock(&(pk_state.lock));
}

void pks_printlog(FILE* dest)
{
  pthread_mutex_lock(&(pk_state.lock));
  fprintf(dest, "%s", pk_state.log_ring_start);
  if (pk_state.log_ring_end < pk_state.log_ring_start)
    fprintf(dest, "%s", pk_state.log_ring_buffer);
  pthread_mutex_unlock(&(pk_state.lock));
}

void pks_free_ssl_cert_names()
{
  char** p = pk_state.ssl_cert_names;
  if ((p != NULL) && (*p != *PAGEKITE_NET_CERT_NAMES)) {
    while (*p != NULL) free(*p++);
    free(pk_state.ssl_cert_names);
  }
  pk_state.ssl_cert_names = NULL;
}

void pks_add_ssl_cert_names(char** names)
{
  int len_current = 0;
  int len_new = 0;

  char** p = pk_state.ssl_cert_names;
  while (p != NULL && *p++ != NULL) len_current++;

  char** n = (char**) names;
  while (n != NULL && *n++ != NULL) len_new++;

  if (!(len_current || len_new)) {
    pks_free_ssl_cert_names();
    return;
  }

  /* Allocate space for some character pointers */
  char** buffer = malloc((len_current + len_new + 1) * sizeof(char*));

  /* Copy the old set of names */
  n = buffer;
  p = pk_state.ssl_cert_names;
  while (p && *p) { *n++ = strdup(*p++); };

  /* Copy the new names */
  p = (char**) names;
  while (p && *p) { *n++ = strdup(*p++); };

  /* End of list marker */
  *n = NULL;

  /* Update pk_state */
  pks_free_ssl_cert_names();
  pk_state.ssl_cert_names = buffer;
}
