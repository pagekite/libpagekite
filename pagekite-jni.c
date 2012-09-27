/******************************************************************************
pagekite-jni.c - JNI wrappers offering a high-level API.

*******************************************************************************

This file is Copyright 2012, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify it under
the terms of the  GNU  Affero General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for more
details.

You should have received a copy of the GNU Affero General Public License
along with th program.  If not, see: <http://www.gnu.org/licenses/>

Note: For alternate license terms, see the file COPYING.md.

******************************************************************************/

#include <jni.h>
#include <android/log.h>

#include "common.h"
#include "utils.h"
#include "pkstate.h"
#include "pkerror.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pklogging.h"
#include "pkblocker.h"
#include "pkmanager.h"

#include "pagekite_net.h"


#define BUFFER_SIZE 512 * 1024
struct pk_global_state pk_state;
struct pk_manager *pk_manager_global = NULL;
char pk_manager_buffer[BUFFER_SIZE];
char pk_app_id_short[128];
char pk_app_id_long[128];


jboolean Java_net_pagekite_lib_PageKiteAPI_init(JNIEnv* env, jclass cl,
  jint jKites, jint jFrontends, jint jConns, jstring jDynDns,
  jstring jAppIdShort, jstring jAppIdLong,
  jboolean jDebug)
{
  int kites = jKites;
  int fe_max = jFrontends;
  int conns = jConns;
  char *buffer;
  const jbyte *is, *il;
  SSL_CTX* ssl_ctx;

  if (pk_manager_global != NULL) return JNI_FALSE;

  const jbyte* dyndns = NULL;
  if (jDynDns != NULL) dyndns = (*env)->GetStringUTFChars(env, jDynDns, NULL);

  if (jDebug) {
    pks_global_init(PK_LOG_ALL);
  }
  else {
    pks_global_init(PK_LOG_NORMAL);
    pk_state.log_file = NULL;
  }

  pk_state.app_id_short = pk_app_id_short;
  pk_state.app_id_long = pk_app_id_long;
  strcpy(pk_app_id_short, is = (*env)->GetStringUTFChars(env, jAppIdShort, NULL));
  strcpy(pk_app_id_long, il = (*env)->GetStringUTFChars(env, jAppIdLong, NULL));
  (*env)->ReleaseStringUTFChars(env, jAppIdShort, is);
  (*env)->ReleaseStringUTFChars(env, jAppIdLong, il);

  PKS_SSL_INIT(ssl_ctx);
  pk_log(PK_LOG_MANAGER_DEBUG, "JNI: Initializing");
  pk_manager_global = pkm_manager_init(NULL, BUFFER_SIZE, pk_manager_buffer,
                                       kites, fe_max, conns, dyndns, ssl_ctx);

  if (dyndns != NULL) (*env)->ReleaseStringUTFChars(env, jDynDns, dyndns);
  if (NULL == pk_manager_global) {
    return JNI_FALSE;
  }
  else {
    pk_manager_global->fancy_pagekite_net_rejection = 0;
    pkm_set_timer_enabled(pk_manager_global, 0);
    pkm_tick(pk_manager_global);
  }
  return JNI_TRUE;
}

jboolean Java_net_pagekite_lib_PageKiteAPI_initPagekiteNet(JNIEnv* env, jclass cl,
  jint jKites, jint jConns,
  jstring jAppIdShort, jstring jAppIdLong,
  jboolean jDebug)
{
  int kites = jKites;
  int conns = jConns;
  char *buffer;
  const jbyte *is, *il;
  SSL_CTX* ssl_ctx;

  if (pk_manager_global != NULL) return JNI_FALSE;

  if (jDebug) {
    pks_global_init(PK_LOG_ALL);
  }
  else {
    pks_global_init(PK_LOG_NORMAL);
    pk_state.log_file = NULL;
  }

  pk_state.app_id_short = pk_app_id_short;
  pk_state.app_id_long = pk_app_id_long;
  strcpy(pk_app_id_short, is = (*env)->GetStringUTFChars(env, jAppIdShort, NULL));
  strcpy(pk_app_id_long, il = (*env)->GetStringUTFChars(env, jAppIdLong, NULL));
  (*env)->ReleaseStringUTFChars(env, jAppIdShort, is);
  (*env)->ReleaseStringUTFChars(env, jAppIdLong, il);

  PKS_SSL_INIT(ssl_ctx);
  pk_log(PK_LOG_MANAGER_DEBUG, "JNI: Initializing");
  pk_manager_global = pkm_manager_init(NULL, BUFFER_SIZE, pk_manager_buffer,
                                       kites,
                                       PAGEKITE_NET_FE_MAX,
                                       conns,
                                       PAGEKITE_NET_DDNS, ssl_ctx);
  if (NULL == pk_manager_global) {
    pk_perror("pagekite-jni");
    return JNI_FALSE;
  }

  if ((0 > (pkm_add_frontend(pk_manager_global,
                             PAGEKITE_NET_V4FRONTENDS, FE_STATUS_AUTO))) ||
      (0 > (pkm_add_frontend(pk_manager_global,
                             PAGEKITE_NET_V6FRONTENDS, FE_STATUS_AUTO))))
  {
    pk_manager_global = NULL;
    pk_perror("pagekite-jni");
    return JNI_FALSE;
  }

  pkm_set_timer_enabled(pk_manager_global, 0);
  pkm_tick(pk_manager_global);

  return JNI_TRUE;
}

jboolean Java_net_pagekite_lib_PageKiteAPI_addKite(JNIEnv* env, jclass cl,
  jstring jProto, jstring jKitename, jint jPort, jstring jSecret,
  jstring jBackend, jint jLport)
{
  const jbyte *proto = (*env)->GetStringUTFChars(env, jProto, NULL);
  const jbyte *kitename = (*env)->GetStringUTFChars(env, jKitename, NULL);
  int port = jPort;
  const jbyte *secret = (*env)->GetStringUTFChars(env, jSecret, NULL);
  const jbyte *backend = (*env)->GetStringUTFChars(env, jBackend, NULL);
  int lport = jLport;

  if (pk_manager_global == NULL) return JNI_FALSE;
  pk_log(PK_LOG_MANAGER_DEBUG, "JNI: Add kite: %s://%s:%d -> %s:%d",
                               proto, kitename, port, backend, lport);
  int rv = (pkm_add_kite(pk_manager_global, proto, kitename, port, secret,
                                            backend, lport) != NULL); 

  (*env)->ReleaseStringUTFChars(env, jProto, proto);
  (*env)->ReleaseStringUTFChars(env, jKitename, kitename);
  (*env)->ReleaseStringUTFChars(env, jSecret, secret);
  (*env)->ReleaseStringUTFChars(env, jBackend, backend);
  return (rv) ? JNI_TRUE : JNI_FALSE;
}

jboolean Java_net_pagekite_lib_PageKiteAPI_addFrontend(JNIEnv* env, jclass cl,
   jstring jDomain, jint jPort)
{
  const jbyte *domain = (*env)->GetStringUTFChars(env, jDomain, NULL);
  int port = jPort;

  if (pk_manager_global == NULL) return JNI_FALSE;
  pk_log(PK_LOG_MANAGER_DEBUG, "JNI: Add frontend: %s:%d", domain, port);
  int rv = (pkm_add_frontend(pk_manager_global,
                             domain, port, FE_STATUS_AUTO) > 0);

  (*env)->ReleaseStringUTFChars(env, jDomain, domain);
  return (rv) ? JNI_TRUE : JNI_FALSE;
}


jboolean Java_net_pagekite_lib_PageKiteAPI_start(JNIEnv* env, jclass cl)
{
  if (pk_manager_global == NULL) return JNI_FALSE;
  pk_log(PK_LOG_MANAGER_DEBUG, "JNI: Starting worker thread.");
  return (0 == pkm_run_in_thread(pk_manager_global));
}

jboolean Java_net_pagekite_lib_PageKiteAPI_stop(JNIEnv* env, jclass cl)
{
  int rv;
  if (pk_manager_global == NULL) return JNI_FALSE;
  pk_log(PK_LOG_MANAGER_DEBUG, "JNI: Stopping worker thread.");
  rv = pkm_stop_thread(pk_manager_global);
  pk_manager_global = NULL;
  free(pk_state.app_id_short);
  free(pk_state.app_id_long);
  pk_state.app_id_short = pk_state.app_id_long = "unknown";
  return (0 == rv) ? JNI_TRUE : JNI_FALSE;
}


jboolean Java_net_pagekite_lib_PageKiteAPI_tick(JNIEnv* env, jclass cl)
{
  if (pk_manager_global == NULL) return JNI_FALSE;
  pkm_tick(pk_manager_global);
  return JNI_TRUE;
}

jboolean Java_net_pagekite_lib_PageKiteAPI_poll(JNIEnv* env, jclass cl,
  jint jTimeout)
{
  int timeout = jTimeout;
  if (pk_manager_global == NULL) return JNI_FALSE;

  pthread_mutex_lock(&(pk_state.lock));
  /* FIXME: Obey the timeout */
  pthread_cond_wait(&(pk_state.cond), &(pk_state.lock));
  pthread_mutex_unlock(&(pk_state.lock));

  return JNI_TRUE;
}


jint Java_net_pagekite_lib_PageKiteAPI_setHaveNetwork(JNIEnv* env, jclass cl,
  jboolean have_network)
{
  if (pk_manager_global == NULL) return -1;
  if (have_network) {
    PKS_STATE(pk_manager_global->status = PK_STATUS_STARTUP);
  }
  else {
    PKS_STATE(pk_manager_global->status = PK_STATUS_NO_NETWORK);
  }
  return pk_manager_global->status;
}


jint Java_net_pagekite_lib_PageKiteAPI_getStatus(JNIEnv* env, jclass cl)
{
  if (pk_manager_global == NULL) return -1;
  return pk_manager_global->status;
}

jint Java_net_pagekite_lib_PageKiteAPI_getLiveFrontends(JNIEnv* env, jclass cl)
{
  if (pk_manager_global == NULL) return -1;
  return pk_state.live_frontends;
}

jint Java_net_pagekite_lib_PageKiteAPI_getLiveStreams(JNIEnv* env, jclass cl)
{
  if (pk_manager_global == NULL) return -1;
  return pk_state.live_streams;
}

jstring Java_net_pagekite_lib_PageKiteAPI_getLog(JNIEnv* env, jclass cl)
{
  char buffer[PKS_LOG_DATA_MAX];
  if (pk_manager_global == NULL) return (*env)->NewStringUTF(env, "Not running.");
  pks_copylog(buffer);
  return (*env)->NewStringUTF(env, buffer);
}

