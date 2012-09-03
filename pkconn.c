/******************************************************************************
pkconn.c - Connection objects

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

#include "common.h"
#include "utils.h"
#include "pkerror.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pklogging.h"


void pkc_reset_conn(struct pk_conn* pkc)
{
  pkc->status = CONN_STATUS_UNKNOWN;
  pkc->activity = time(0);
  pkc->out_buffer_pos = 0;
  pkc->in_buffer_pos = 0;
  pkc->send_window_kb = CONN_WINDOW_SIZE_KB_MAXIMUM/2;
  pkc->read_bytes = 0;
  pkc->read_kb = 0;
  pkc->sent_kb = 0;
  if (pkc->sockfd >= 0) close(pkc->sockfd);
  pkc->sockfd = -1;
  pkc->state_r = CONN_CLEAR_DATA;
  pkc->state_w = CONN_CLEAR_DATA;
#ifdef HAVE_OPENSSL
  if (pkc->ssl) SSL_free(pkc->ssl);
#endif
}

ssize_t pkc_read_data(struct pk_conn* pkc)
{
  ssize_t bytes, delta;
  bytes = read(pkc->sockfd, PKC_IN(*pkc), PKC_IN_FREE(*pkc));

  if (bytes > 0) {
    pkc->in_buffer_pos += bytes;

    /* Update KB counter and window... this is a bit messy. */
    pkc->read_bytes += bytes;
    while (pkc->read_bytes > 1024) {
      pkc->read_kb += 1;
      pkc->read_bytes -= 1024;
      if ((pkc->read_kb & 0x1f) == 0x00) {
        delta = (CONN_WINDOW_SIZE_KB_MAXIMUM - pkc->send_window_kb);
        if (delta < 0) delta = 0;
        pkc->send_window_kb += (delta/CONN_WINDOW_SIZE_STEPFACTOR);
      }
    }
  }
  else if (bytes == 0) {
    pkc->status |= CONN_STATUS_CLS_READ;
  }
  else switch (errno) {
    case EINTR:
    case EAGAIN:
      break;
    default:
      pkc->status |= CONN_STATUS_BROKEN;
      break;
  }
  return bytes;
}

ssize_t pkc_flush(struct pk_conn* pkc, char *data, ssize_t length, int mode,
                  char* where)
{
  ssize_t flushed, wrote, bytes;
  flushed = wrote = errno = bytes = 0;

  if (pkc->sockfd < 0) {
    pk_log(PK_LOG_BE_DATA|PK_LOG_TUNNEL_DATA, "%d[%s]: Bogus flush?",
                                              pkc->sockfd, where);
    return -1;
  }

  if (mode == BLOCKING_FLUSH) {
    pk_log(PK_LOG_BE_DATA|PK_LOG_TUNNEL_DATA, "%d[%s]: Attempting blocking flush",
                                              pkc->sockfd, where);
    set_blocking(pkc->sockfd);
  }

  /* First, flush whatever was in the conn buffers */
  do {
    wrote = write(pkc->sockfd, pkc->out_buffer, pkc->out_buffer_pos);
    if (wrote > 0) {
      if (pkc->out_buffer_pos > wrote) {
        memmove(pkc->out_buffer,
                pkc->out_buffer + wrote,
                pkc->out_buffer_pos - wrote);
      }
      pkc->out_buffer_pos -= wrote;
      flushed += wrote;
    }
  } while ((errno == EINTR) ||
           ((mode == BLOCKING_FLUSH) && (pkc->out_buffer_pos > 0)));

  /* At this point we either have a non-EINTR error, or we've flushed
   * everything.  Return errors, else continue. */
  if (wrote < 0) {
    flushed = wrote;
    if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
      pkc->status |= CONN_STATUS_CLS_WRITE;
  }
  else if ((NULL != data) &&
           (mode == BLOCKING_FLUSH) &&
           (pkc->out_buffer_pos == 0)) {
    /* So far so good, everything has been flushed. Write the new data! */
    flushed = wrote = 0;
    while ((length > wrote) || (errno == EINTR)) {
      bytes = write(pkc->sockfd, data+wrote, length-wrote);
      if (bytes > 0) {
        wrote += bytes;
        flushed += bytes;
      }
    }
    /* At this point, if we have a non-EINTR error, bytes is < 0 and we
     * want to return that.  Otherwise, return how much got written. */
    if (bytes < 0) {
      flushed = bytes;
      if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
        pkc->status |= CONN_STATUS_CLS_WRITE;
    }
  }

  if (mode == BLOCKING_FLUSH) {
    set_non_blocking(pkc->sockfd);
    pk_log(PK_LOG_BE_DATA|PK_LOG_TUNNEL_DATA, "%d[%s]: Blocking flush complete.",
                                              pkc->sockfd, where);
  }
  return flushed;
}

ssize_t pkc_write_data(struct pk_conn* pkc, ssize_t length, char* data)
{
  ssize_t wleft;
  ssize_t wrote = 0;

  /* 1. Try to flush already buffered data. */
  if (pkc->out_buffer_pos)
    pkc_flush(pkc, NULL, 0, NON_BLOCKING_FLUSH, "pkc_write_data/1");

  /* 2. If successful, try to write new data (0 copies!) */
  if (0 == pkc->out_buffer_pos) {
    errno = 0;
    do {
      wrote = write(pkc->sockfd, data, length);
    } while ((wrote < 0) && (errno == EINTR));
  }

  if (wrote < length) {
    if (wrote < 0) /* Ignore errors, for now */
      wrote = 0;

    wleft = length-wrote;
    if (wleft <= PKC_OUT_FREE(*pkc)) {
      /* 2a. Have data left, there is space in our buffer: buffer it! */
      memcpy(PKC_OUT(*pkc), data+wrote, wleft);
      pkc->out_buffer_pos += wleft;
    }
    else {
      /* 2b. If new+old data > buffer size, do a blocking write. */
      pkc_flush(pkc, data+wrote, length-wrote, BLOCKING_FLUSH,
                "pkc_write_data/2");
    }
  }

  return length;
}
