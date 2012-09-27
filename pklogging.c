/******************************************************************************
pklogging.c - Logging.

This file is Copyright 2011, 2012, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify it under
the terms  of the  Apache  License 2.0  as published by the  Apache  Software
Foundation.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
details.

You should have received a copy of the Apache License along with this program.
If not, see: <http://www.apache.org/licenses/>

Note: For alternate license terms, see the file COPYING.md.

******************************************************************************/

#include "common.h"

#ifdef ANDROID
#include <android/log.h>
#endif

#include "pkstate.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pklogging.h"

static int logged_lines = 0;


int pk_log(int level, const char* fmt, ...)
{
  va_list args;
  char output[4000];
  int r, len;
  FILE* log_file;

  if (level & pk_state.log_mask) {
    len = sprintf(output, "ts=%x; tid=%x; ll=%x; lm=%x; msg=",
                          (int) time(0), (int) pthread_self(),
                          logged_lines++, level);
    va_start(args, fmt);
    len += (r = vsnprintf(output + len, 4000 - len, fmt, args));
    va_end(args);

    if (r > 0) {
      pks_logcopy(output, len);
      log_file = pk_state.log_file; /* Avoid race conditions if it changes. */
      if (log_file != NULL) {
#ifdef ANDROID
#warning Default logging uses __android_log_print instead of stderr.
        if (log_file == stderr) {
          __android_log_print(ANDROID_LOG_INFO,
                              "libpagekite", "%.4000s\n", output);
        } else
#endif
          fprintf(log_file, "%.4000s\n", output);
      }
    }
  }
  else {
    r = 0;
  }

  return r;
}

int pk_log_chunk(struct pk_chunk* chnk) {
  int i;
  int r = 0;
  if (chnk->ping) {
    r += pk_log(PK_LOG_TUNNEL_HEADERS, "PING");
  }
  else if (chnk->sid) {
    if (chnk->noop) {
      r += pk_log(PK_LOG_TUNNEL_DATA, "[sid=%s] NOOP: (eof:%d skb:%d spd:%d)",
                                      chnk->sid, chnk->eof,
                                      chnk->remote_sent_kb, chnk->throttle_spd);
    }
    else if (chnk->eof) {
      r += pk_log(PK_LOG_TUNNEL_DATA, "[sid=%s] EOF: %s", chnk->sid, chnk->eof);
    }
    else {
      if (chnk->request_host) {
        r += pk_log(PK_LOG_TUNNEL_CONNS,
                    "[%s]:%d requested %s://%s:%d%s [sid=%s]",
                    chnk->remote_ip, chnk->remote_port,
                    chnk->request_proto, chnk->request_host, chnk->request_port,
                    chnk->remote_tls ? " (encrypted)" : "", chnk->sid);
      }
      r += pk_log(PK_LOG_TUNNEL_DATA, "[sid=%s] DATA: %d bytes",
                                      chnk->sid, chnk->length);
    }
  }
  else {
    r += pk_log(PK_LOG_TUNNEL_HEADERS, "Weird: Non-ping chnk with no SID");
  }
  for (i = 0; i < chnk->header_count; i++) {
    r += pk_log(PK_LOG_TUNNEL_HEADERS, "Header: %s", chnk->headers[i]);
  }
  return r;
}
