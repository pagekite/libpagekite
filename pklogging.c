/******************************************************************************
pklogging.c - Logging.

This file is Copyright 2011, 2012, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify it under
the terms of the  GNU  Affero General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for more
details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see: <http://www.gnu.org/licenses/>

Note: For alternate license terms, see the file COPYING.md.

******************************************************************************/
#include <assert.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef ANDROID
#include <android/log.h>
#endif

#include "pkstate.h"
#include "pkproto.h"
#include "pklogging.h"


int pk_log(int level, const char* fmt, ...)
{
  va_list args;
  char output[4000];
  int r;

  if (level & pk_state.log_mask) {
    va_start(args, fmt);
    r = vsnprintf(output, 4000, fmt, args); 
    va_end(args);
    if (r > 0) {
#ifdef ANDROID
#warning Logging uses Android __android_log_print.
      __android_log_print(ANDROID_LOG_INFO, "libpagekite", "[%d] %.4000s\n",
                          time(0), output);
#else
      fprintf(stderr, "%.4000s\n", output);
#endif
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
    r += pk_log(PK_LOG_TUNNEL_HEADERS, "< --- < Ping?");
  }
  else if (chnk->sid) {
    if (chnk->noop) {
      r += pk_log(PK_LOG_TUNNEL_DATA, "<%5.5s< NOOP: (eof:%d skb:%d spd:%d)",
                                      chnk->sid, chnk->eof,
                                      chnk->remote_sent_kb, chnk->throttle_spd);
    }
    else if (chnk->eof) {
      r += pk_log(PK_LOG_TUNNEL_DATA, "<%5.5s< EOF: %s", chnk->sid, chnk->eof);
    }
    else {
      if (chnk->request_host) {
        r += pk_log(PK_LOG_TUNNEL_CONNS,
                    "<%5.5s<  %s:%d requested %s://%s:%d%s",
                    chnk->sid, chnk->remote_ip, chnk->remote_port,
                    chnk->request_proto, chnk->request_host, chnk->request_port,
                    chnk->remote_tls ? " (encrypted)" : "");
      }
      r += pk_log(PK_LOG_TUNNEL_DATA, "<%5.5s< DATA: %d bytes",
                                      chnk->sid, chnk->length);
    }
  }
  else {
    r += pk_log(PK_LOG_TUNNEL_HEADERS, "< ??? < Non-ping chnk with no SID");
  }
  for (i = 0; i < chnk->header_count; i++) {
    r += pk_log(PK_LOG_TUNNEL_HEADERS, "<-----< %s", chnk->headers[i]);
  }
  return r;
}
