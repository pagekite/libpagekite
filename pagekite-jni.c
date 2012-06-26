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

******************************************************************************/
#include <assert.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>

#include <ev.h>
#include <jni.h>
#include <android/log.h>

#include "utils.h"
#include "pkstate.h"
#include "pkerror.h"
#include "pkproto.h"
#include "pklogging.h"
#include "pkmanager.h"


#define BUFFER_SIZE 256 * 1024
struct pk_global_state pk_state;
struct pk_manager *pk_manager_global = NULL;
char pk_manager_buffer[BUFFER_SIZE];


jboolean Java_net_pagekite_lib_PageKiteAPI_init(JNIEnv* env, jclass cl,
  jint jKites, jint jFrontends, jint jConns)
{
  int kites = jKites;
  int frontends = jFrontends;
  int conns = jConns;
  char *buffer;

  if (pk_manager_global != NULL) return JNI_FALSE;

  pk_state.log_mask = PK_LOG_ALL;
  pk_log(PK_LOG_MANAGER_DEBUG, "JNI: Initializing");
  pk_manager_global = pkm_manager_init(NULL, BUFFER_SIZE, pk_manager_buffer,
                                       kites, frontends, conns);
  if (NULL == pk_manager_global) {
    return JNI_FALSE;
  }
  return JNI_TRUE;
}

jboolean Java_net_pagekite_lib_PageKiteAPI_free(JNIEnv* env, jclass cl)
{
  if (pk_manager_global != NULL) {
    pk_log(PK_LOG_MANAGER_DEBUG, "JNI: Cleaning up");
    pk_manager_global = NULL;
    return JNI_TRUE;
  }
  return JNI_FALSE;
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
  return rv;
}

jboolean Java_net_pagekite_lib_PageKiteAPI_addFrontend(JNIEnv* env, jclass cl,
   jstring jDomain, jint jPort, jint jPrio)
{
  const jbyte *domain = (*env)->GetStringUTFChars(env, jDomain, NULL);
  int port = jPort;
  int prio = jPrio;

  if (pk_manager_global == NULL) return JNI_FALSE;
  pk_log(PK_LOG_MANAGER_DEBUG, "JNI: Add frontend: %s:%d", domain, port);
  int rv = (pkm_add_frontend(pk_manager_global, domain, port, prio) != NULL); 

  (*env)->ReleaseStringUTFChars(env, jDomain, domain);
  return rv;
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
  return (0 == rv);
}

