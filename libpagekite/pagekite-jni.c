/* ****************************************************************************
   pagekite-jni.c - Wrappers for the libpagekite API

      * * *   WARNING: This file is auto-generated, do not edit!  * * *

*******************************************************************************
This file is Copyright 2012-2020, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify it under
the terms  of the  Apache  License 2.0  as published by the  Apache  Software
Foundation.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the Apache License for more details.

You should have received a copy of the Apache License along with this program.
If not, see: <http://www.apache.org/licenses/>

Note: For alternate license terms, see the file COPYING.md.
**************************************************************************** */

#include "config.h"
#if HAVE_RELAY
#define WITH_PAGEKITE_RELAY 1
#endif

#include "pagekite.h"
#include "pkcommon.h"

#ifdef HAVE_JNI_H
#include <jni.h>

static pagekite_mgr pagekite_manager_global = NULL;

jboolean Java_net_pagekite_lib_PageKiteAPI_init(
  JNIEnv* env, jclass unused_class
, jstring japp_id
, jint jmax_kites
, jint jmax_frontends
, jint jmax_conns
, jstring jdyndns_url
, jint jflags
, jint jverbosity
){
  if (pagekite_manager_global != NULL) return JNI_FALSE;

  const jbyte* app_id = NULL;
  if (japp_id != NULL) app_id = (*env)->GetStringUTFChars(env, japp_id, NULL);
  int max_kites = jmax_kites;
  int max_frontends = jmax_frontends;
  int max_conns = jmax_conns;
  const jbyte* dyndns_url = NULL;
  if (jdyndns_url != NULL) dyndns_url = (*env)->GetStringUTFChars(env, jdyndns_url, NULL);
  int flags = jflags;
  int verbosity = jverbosity;

  pagekite_manager_global = pagekite_init(app_id, max_kites, max_frontends, max_conns, dyndns_url, flags, verbosity);

  if (japp_id != NULL) (*env)->ReleaseStringUTFChars(env, japp_id, app_id);
  if (jdyndns_url != NULL) (*env)->ReleaseStringUTFChars(env, jdyndns_url, dyndns_url);
  return (pagekite_manager_global != NULL);
}

jboolean Java_net_pagekite_lib_PageKiteAPI_initPagekitenet(
  JNIEnv* env, jclass unused_class
, jstring japp_id
, jint jmax_kites
, jint jmax_conns
, jint jflags
, jint jverbosity
){
  if (pagekite_manager_global != NULL) return JNI_FALSE;

  const jbyte* app_id = NULL;
  if (japp_id != NULL) app_id = (*env)->GetStringUTFChars(env, japp_id, NULL);
  int max_kites = jmax_kites;
  int max_conns = jmax_conns;
  int flags = jflags;
  int verbosity = jverbosity;

  pagekite_manager_global = pagekite_init_pagekitenet(app_id, max_kites, max_conns, flags, verbosity);

  if (japp_id != NULL) (*env)->ReleaseStringUTFChars(env, japp_id, app_id);
  return (pagekite_manager_global != NULL);
}

jboolean Java_net_pagekite_lib_PageKiteAPI_initWhitelabel(
  JNIEnv* env, jclass unused_class
, jstring japp_id
, jint jmax_kites
, jint jmax_conns
, jint jflags
, jint jverbosity
, jstring jwhitelabel_tld
){
  if (pagekite_manager_global != NULL) return JNI_FALSE;

  const jbyte* app_id = NULL;
  if (japp_id != NULL) app_id = (*env)->GetStringUTFChars(env, japp_id, NULL);
  int max_kites = jmax_kites;
  int max_conns = jmax_conns;
  int flags = jflags;
  int verbosity = jverbosity;
  const jbyte* whitelabel_tld = NULL;
  if (jwhitelabel_tld != NULL) whitelabel_tld = (*env)->GetStringUTFChars(env, jwhitelabel_tld, NULL);

  pagekite_manager_global = pagekite_init_whitelabel(app_id, max_kites, max_conns, flags, verbosity, whitelabel_tld);

  if (japp_id != NULL) (*env)->ReleaseStringUTFChars(env, japp_id, app_id);
  if (jwhitelabel_tld != NULL) (*env)->ReleaseStringUTFChars(env, jwhitelabel_tld, whitelabel_tld);
  return (pagekite_manager_global != NULL);
}

jint Java_net_pagekite_lib_PageKiteAPI_addKite(
  JNIEnv* env, jclass unused_class
, jstring jproto
, jstring jkitename
, jint jpport
, jstring jsecret
, jstring jbackend
, jint jlport
){
  if (pagekite_manager_global == NULL) return -1;

  const jbyte* proto = NULL;
  if (jproto != NULL) proto = (*env)->GetStringUTFChars(env, jproto, NULL);
  const jbyte* kitename = NULL;
  if (jkitename != NULL) kitename = (*env)->GetStringUTFChars(env, jkitename, NULL);
  int pport = jpport;
  const jbyte* secret = NULL;
  if (jsecret != NULL) secret = (*env)->GetStringUTFChars(env, jsecret, NULL);
  const jbyte* backend = NULL;
  if (jbackend != NULL) backend = (*env)->GetStringUTFChars(env, jbackend, NULL);
  int lport = jlport;

  jint rv = pagekite_add_kite(pagekite_manager_global, proto, kitename, pport, secret, backend, lport);

  if (jproto != NULL) (*env)->ReleaseStringUTFChars(env, jproto, proto);
  if (jkitename != NULL) (*env)->ReleaseStringUTFChars(env, jkitename, kitename);
  if (jsecret != NULL) (*env)->ReleaseStringUTFChars(env, jsecret, secret);
  if (jbackend != NULL) (*env)->ReleaseStringUTFChars(env, jbackend, backend);
  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_addServiceFrontends(
  JNIEnv* env, jclass unused_class
, jint jflags
){
  if (pagekite_manager_global == NULL) return -1;

  int flags = jflags;

  jint rv = pagekite_add_service_frontends(pagekite_manager_global, flags);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_addWhitelabelFrontends(
  JNIEnv* env, jclass unused_class
, jint jflags
, jstring jwhitelabel_tld
){
  if (pagekite_manager_global == NULL) return -1;

  int flags = jflags;
  const jbyte* whitelabel_tld = NULL;
  if (jwhitelabel_tld != NULL) whitelabel_tld = (*env)->GetStringUTFChars(env, jwhitelabel_tld, NULL);

  jint rv = pagekite_add_whitelabel_frontends(pagekite_manager_global, flags, whitelabel_tld);

  if (jwhitelabel_tld != NULL) (*env)->ReleaseStringUTFChars(env, jwhitelabel_tld, whitelabel_tld);
  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_lookupAndAddFrontend(
  JNIEnv* env, jclass unused_class
, jstring jdomain
, jint jport
, jint jupdate_from_dns
){
  if (pagekite_manager_global == NULL) return -1;

  const jbyte* domain = NULL;
  if (jdomain != NULL) domain = (*env)->GetStringUTFChars(env, jdomain, NULL);
  int port = jport;
  int update_from_dns = jupdate_from_dns;

  jint rv = pagekite_lookup_and_add_frontend(pagekite_manager_global, domain, port, update_from_dns);

  if (jdomain != NULL) (*env)->ReleaseStringUTFChars(env, jdomain, domain);
  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_addFrontend(
  JNIEnv* env, jclass unused_class
, jstring jdomain
, jint jport
){
  if (pagekite_manager_global == NULL) return -1;

  const jbyte* domain = NULL;
  if (jdomain != NULL) domain = (*env)->GetStringUTFChars(env, jdomain, NULL);
  int port = jport;

  jint rv = pagekite_add_frontend(pagekite_manager_global, domain, port);

  if (jdomain != NULL) (*env)->ReleaseStringUTFChars(env, jdomain, domain);
  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_setLogMask(
  JNIEnv* env, jclass unused_class
, jint jmask
){
  if (pagekite_manager_global == NULL) return -1;

  int mask = jmask;

  jint rv = pagekite_set_log_mask(pagekite_manager_global, mask);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_setLogDestination(
  JNIEnv* env, jclass unused_class
, jint jlog_destination
){
  if (pagekite_manager_global == NULL) return -1;

  int log_destination = jlog_destination;

  jint rv = pagekite_set_log_destination(pagekite_manager_global, log_destination);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_setHousekeepingMinInterval(
  JNIEnv* env, jclass unused_class
, jint jinterval
){
  if (pagekite_manager_global == NULL) return -1;

  int interval = jinterval;

  jint rv = pagekite_set_housekeeping_min_interval(pagekite_manager_global, interval);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_setHousekeepingMaxInterval(
  JNIEnv* env, jclass unused_class
, jint jinterval
){
  if (pagekite_manager_global == NULL) return -1;

  int interval = jinterval;

  jint rv = pagekite_set_housekeeping_max_interval(pagekite_manager_global, interval);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_setRejectionUrl(
  JNIEnv* env, jclass unused_class
, jstring jurl
){
  if (pagekite_manager_global == NULL) return -1;

  const jbyte* url = NULL;
  if (jurl != NULL) url = (*env)->GetStringUTFChars(env, jurl, NULL);

  jint rv = pagekite_set_rejection_url(pagekite_manager_global, url);

  if (jurl != NULL) (*env)->ReleaseStringUTFChars(env, jurl, url);
  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_enableHttpForwardingHeaders(
  JNIEnv* env, jclass unused_class
, jint jenable
){
  if (pagekite_manager_global == NULL) return -1;

  int enable = jenable;

  jint rv = pagekite_enable_http_forwarding_headers(pagekite_manager_global, enable);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_enableFakePing(
  JNIEnv* env, jclass unused_class
, jint jenable
){
  if (pagekite_manager_global == NULL) return -1;

  int enable = jenable;

  jint rv = pagekite_enable_fake_ping(pagekite_manager_global, enable);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_enableWatchdog(
  JNIEnv* env, jclass unused_class
, jint jenable
){
  if (pagekite_manager_global == NULL) return -1;

  int enable = jenable;

  jint rv = pagekite_enable_watchdog(pagekite_manager_global, enable);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_enableTickTimer(
  JNIEnv* env, jclass unused_class
, jint jenable
){
  if (pagekite_manager_global == NULL) return -1;

  int enable = jenable;

  jint rv = pagekite_enable_tick_timer(pagekite_manager_global, enable);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_setConnEvictionIdleS(
  JNIEnv* env, jclass unused_class
, jint jseconds
){
  if (pagekite_manager_global == NULL) return -1;

  int seconds = jseconds;

  jint rv = pagekite_set_conn_eviction_idle_s(pagekite_manager_global, seconds);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_setOpensslCiphers(
  JNIEnv* env, jclass unused_class
, jstring jciphers
){
  if (pagekite_manager_global == NULL) return -1;

  const jbyte* ciphers = NULL;
  if (jciphers != NULL) ciphers = (*env)->GetStringUTFChars(env, jciphers, NULL);

  jint rv = pagekite_set_openssl_ciphers(pagekite_manager_global, ciphers);

  if (jciphers != NULL) (*env)->ReleaseStringUTFChars(env, jciphers, ciphers);
  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_wantSpareFrontends(
  JNIEnv* env, jclass unused_class
, jint jspares
){
  if (pagekite_manager_global == NULL) return -1;

  int spares = jspares;

  jint rv = pagekite_want_spare_frontends(pagekite_manager_global, spares);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_threadStart(
  JNIEnv* env, jclass unused_class
){
  if (pagekite_manager_global == NULL) return -1;


  jint rv = pagekite_thread_start(pagekite_manager_global);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_threadWait(
  JNIEnv* env, jclass unused_class
){
  if (pagekite_manager_global == NULL) return -1;


  jint rv = pagekite_thread_wait(pagekite_manager_global);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_threadStop(
  JNIEnv* env, jclass unused_class
){
  if (pagekite_manager_global == NULL) return -1;


  jint rv = pagekite_thread_stop(pagekite_manager_global);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_free(
  JNIEnv* env, jclass unused_class
){
  if (pagekite_manager_global == NULL) return -1;


  jint rv = pagekite_free(pagekite_manager_global);

  pagekite_manager_global = NULL;
  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_setEventMask(
  JNIEnv* env, jclass unused_class
, jint jmask
){
  if (pagekite_manager_global == NULL) return -1;

  unsigned int mask = jmask;

  jint rv = pagekite_set_event_mask(pagekite_manager_global, mask);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_awaitEvent(
  JNIEnv* env, jclass unused_class
, jint jtimeout
){
  if (pagekite_manager_global == NULL) return -1;

  int timeout = jtimeout;

  jint rv = pagekite_await_event(pagekite_manager_global, timeout);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_getEventInt(
  JNIEnv* env, jclass unused_class
, jint jevent_code
){
  if (pagekite_manager_global == NULL) return -1;

  unsigned int event_code = jevent_code;

  jint rv = pagekite_get_event_int(pagekite_manager_global, event_code);

  return rv;
}

jstring Java_net_pagekite_lib_PageKiteAPI_getEventStr(
  JNIEnv* env, jclass unused_class
, jint jevent_code
){
  if (pagekite_manager_global == NULL) return NULL;

  unsigned int event_code = jevent_code;

  jstring rv = (*env)->NewStringUTF(env, pagekite_get_event_str(pagekite_manager_global, event_code));

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_eventRespond(
  JNIEnv* env, jclass unused_class
, jint jevent_code
, jint jresponse_code
){
  if (pagekite_manager_global == NULL) return -1;

  unsigned int event_code = jevent_code;
  unsigned int response_code = jresponse_code;

  jint rv = pagekite_event_respond(pagekite_manager_global, event_code, response_code);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_eventRespondWithData(
  JNIEnv* env, jclass unused_class
, jint jevent_code
, jint jresponse_code
, jint jresponse_int
, jstring jresponse_str
){
  if (pagekite_manager_global == NULL) return -1;

  unsigned int event_code = jevent_code;
  unsigned int response_code = jresponse_code;
  int          response_int = jresponse_int;
  const jbyte* response_str = NULL;
  if (jresponse_str != NULL) response_str = (*env)->GetStringUTFChars(env, jresponse_str, NULL);

  jint rv = pagekite_event_respond_with_data(pagekite_manager_global, event_code, response_code, response_int, response_str);

  if (jresponse_str != NULL) (*env)->ReleaseStringUTFChars(env, jresponse_str, response_str);
  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_getStatus(
  JNIEnv* env, jclass unused_class
){
  if (pagekite_manager_global == NULL) return -1;


  jint rv = pagekite_get_status(pagekite_manager_global);

  return rv;
}

jstring Java_net_pagekite_lib_PageKiteAPI_getLog(
  JNIEnv* env, jclass unused_class
){
  if (pagekite_manager_global == NULL) return NULL;


  jstring rv = (*env)->NewStringUTF(env, pagekite_get_log(pagekite_manager_global));

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_dumpStateToLog(
  JNIEnv* env, jclass unused_class
){
  if (pagekite_manager_global == NULL) return -1;


  jint rv = pagekite_dump_state_to_log(pagekite_manager_global);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_poll(
  JNIEnv* env, jclass unused_class
, jint jtimeout
){
  if (pagekite_manager_global == NULL) return -1;

  int timeout = jtimeout;

  jint rv = pagekite_poll(pagekite_manager_global, timeout);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_tick(
  JNIEnv* env, jclass unused_class
){
  if (pagekite_manager_global == NULL) return -1;


  jint rv = pagekite_tick(pagekite_manager_global);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_setBailOnErrors(
  JNIEnv* env, jclass unused_class
, jint jerrors
){
  if (pagekite_manager_global == NULL) return -1;

  int errors = jerrors;

  jint rv = pagekite_set_bail_on_errors(pagekite_manager_global, errors);

  return rv;
}

jint Java_net_pagekite_lib_PageKiteAPI_perror(
  JNIEnv* env, jclass unused_class
, jstring jprefix
){
  if (pagekite_manager_global == NULL) return -1;

  const jbyte* prefix = NULL;
  if (jprefix != NULL) prefix = (*env)->GetStringUTFChars(env, jprefix, NULL);

  jint rv = pagekite_perror(pagekite_manager_global, prefix);

  if (jprefix != NULL) (*env)->ReleaseStringUTFChars(env, jprefix, prefix);
  return rv;
}
#else
#  warning Java not found, not building JNI.
#endif