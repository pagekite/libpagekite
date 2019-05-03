#ifdef HAVE_OPENSSL
/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2017, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/
/* <DESC>
 * one way to set the necessary OpenSSL locking callbacks if you want to do
 * multi-threaded transfers with HTTPS/FTPS with libcurl built to use OpenSSL.
 * </DESC>
 */
/*
 * This is not a complete stand-alone example.
 *
 * Author: Jeremy Brown
 */

/*
 * Modifications for libpagekite made by Bjarni R. Einarsson.
 */
#ifndef _MSC_VER

#include <stdio.h>
#include <pthread.h>
#include <openssl/err.h>

#define MUTEX_TYPE       pthread_mutex_t
#define MUTEX_SETUP(x)   pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x)    pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x)  pthread_mutex_unlock(&(x))
#define THREAD_ID        pthread_self()

#if OPENSSL_VERSION_NUMBER < 0x10000000L
#define CRYPTO_THREADID_set_callback CRYPTO_set_id_callback
#endif

#else

/* FIXME: Make this work on Windows */

#endif


void handle_error(const char *file, int lineno, const char *msg)
{
  fprintf(stderr, "** %s:%d %s\n", file, lineno, msg);
  ERR_print_errors_fp(stderr);
  /* exit(-1); */
}

/* This array will store all of the mutexes available to OpenSSL. */
static MUTEX_TYPE *pk_ssl_mutex_buf = NULL;

static void pk_ssl_locking_function(int mode, int n, const char *file, int line)
{
  if(mode & CRYPTO_LOCK)
    MUTEX_LOCK(pk_ssl_mutex_buf[n]);
  else
    MUTEX_UNLOCK(pk_ssl_mutex_buf[n]);
}

#if OPENSSL_VERSION_NUMBER < 0x10000000L
static unsigned long pk_ssl_id_function(void)
{
  return ((unsigned long)THREAD_ID);
}
#else
static void pk_ssl_id_function(CRYPTO_THREADID *id)
{
  CRYPTO_THREADID_set_numeric(id, (unsigned long)THREAD_ID);
}
#endif

#endif

int pk_ssl_thread_setup(void)
{
#ifdef HAVE_OPENSSL
  int i;
  pk_ssl_mutex_buf = malloc(CRYPTO_num_locks() * sizeof(MUTEX_TYPE));
  if(!pk_ssl_mutex_buf)
    return 0;
  for(i = 0;  i < CRYPTO_num_locks();  i++)
    MUTEX_SETUP(pk_ssl_mutex_buf[i]);
  CRYPTO_THREADID_set_callback(pk_ssl_id_function);
  CRYPTO_set_locking_callback(pk_ssl_locking_function);
#endif
  return 1;
}

int pk_ssl_thread_cleanup(void)
{
#ifdef HAVE_OPENSSL
  int i;
  if(!mutex_buf)
    return 0;
  CRYPTO_THREADID_set_callback(NULL);
  CRYPTO_set_locking_callback(NULL);
  for(i = 0;  i < CRYPTO_num_locks();  i++)
    MUTEX_CLEANUP(pk_ssl_mutex_buf[i]);
  free(pk_ssl_mutex_buf);
  pk_ssl_mutex_buf = NULL;
#endif
  return 1;
}
