/******************************************************************************
pkconn.c - Connection objects

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
#include <poll.h>

#include "utils.h"
#include "pkerror.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pklogging.h"


void pkc_reset_conn(struct pk_conn* pkc)
{
  pkc->status &= ~CONN_STATUS_BITS;
  pkc->activity = time(0);
  pkc->out_buffer_pos = 0;
  pkc->in_buffer_pos = 0;
  pkc->send_window_kb = CONN_WINDOW_SIZE_KB_MAXIMUM/2;
  pkc->read_bytes = 0;
  pkc->read_kb = 0;
  pkc->sent_kb = 0;
  if (pkc->sockfd >= 0) close(pkc->sockfd);
  pkc->sockfd = -1;
  pkc->state = CONN_CLEAR_DATA;
#ifdef HAVE_OPENSSL
  if (pkc->ssl) SSL_free(pkc->ssl);
  pkc->ssl = NULL;
#endif
}

int pkc_connect(struct pk_conn* pkc, struct addrinfo* ai)
{
  int fd;
  pkc_reset_conn(pkc);
  if ((0 > (fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol))) ||
      (0 > connect(fd, ai->ai_addr, ai->ai_addrlen))) {
    pkc->sockfd = -1;
    if (fd >= 0) close(fd);
    return (pk_error = ERR_CONNECT_CONNECT);
  }

  /* FIXME: Add support for chaining through socks or HTTP proxies */

  return (pkc->sockfd = fd);
}

#ifdef HAVE_OPENSSL
static void pkc_start_handshake(struct pk_conn* pkc, int err)
{
  pk_log(PK_LOG_BE_DATA|PK_LOG_TUNNEL_DATA,
         "%d: Started SSL handshake", pkc->sockfd);

  pkc->state = CONN_SSL_HANDSHAKE;
  if (err == SSL_ERROR_WANT_READ) {
    pkc->status |= CONN_STATUS_WANT_READ;
  }
  else if (err == SSL_ERROR_WANT_WRITE) {
    pkc->status |= CONN_STATUS_WANT_WRITE;
  }
}

static void pkc_end_handshake(struct pk_conn *pkc)
{
  pk_log(PK_LOG_BE_DATA|PK_LOG_TUNNEL_DATA,
         "%d: Finished SSL handshake", pkc->sockfd);
  pkc->status &= ~(CONN_STATUS_WANT_WRITE|CONN_STATUS_WANT_READ);
  pkc->state = CONN_SSL_DATA;
}

static void pkc_do_handshake(struct pk_conn *pkc)
{
  int rv = SSL_do_handshake(pkc->ssl);
  if (rv == 1) {
    pkc_end_handshake(pkc);
  }
  else {
    int err = SSL_get_error(pkc->ssl, rv);
    switch (err) {
      case SSL_ERROR_WANT_READ:
        pkc->status |= CONN_STATUS_WANT_READ;
        break;
      case SSL_ERROR_WANT_WRITE:
        pkc->status |= CONN_STATUS_WANT_WRITE;
        break;
      case SSL_ERROR_ZERO_RETURN:
        pkc->status |= CONN_STATUS_BROKEN;
        break;
      default:
        pkc->status |= CONN_STATUS_BROKEN;
        break;
    }
  }
}

int pkc_start_ssl(struct pk_conn* pkc, SSL_CTX* ctx)
{
  long mode;
  pkc->ssl = SSL_new(ctx);
  /* FIXME: Error checking? */

  mode = SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER;
  mode |= SSL_MODE_ENABLE_PARTIAL_WRITE;
#ifdef SSL_MODE_RELEASE_BUFFERS
  mode |= SSL_MODE_RELEASE_BUFFERS;
#endif
  SSL_set_mode(pkc->ssl, mode);
  SSL_set_connect_state(pkc->ssl);
  SSL_set_app_data(pkc->ssl, pkc);
  SSL_set_fd(pkc->ssl, pkc->sockfd);

  pkc_start_handshake(pkc, SSL_ERROR_WANT_WRITE);
  pkc_do_handshake(pkc);

  return -1;
}
#endif

int pkc_wait(struct pk_conn* pkc, int timeout)
{
  int rv;
  struct pollfd pfd;
  set_non_blocking(pkc->sockfd);
  pfd.fd = pkc->sockfd;
  pfd.events = (POLLIN | POLLPRI | POLLHUP);
  do {
    rv = poll(&pfd, 1, timeout);
  } while ((rv < 0) && (errno == EINTR));
  return rv;
}

ssize_t pkc_read(struct pk_conn* pkc)
{
  ssize_t bytes, delta;
  int ssl_errno = SSL_ERROR_NONE;

  switch (pkc->state) {
#ifdef HAVE_OPENSSL
    case CONN_SSL_DATA:
      bytes = SSL_read(pkc->ssl, PKC_IN(*pkc), PKC_IN_FREE(*pkc));
      if (bytes < 0) ssl_errno = SSL_get_error(pkc->ssl, bytes);
      break;
    case CONN_SSL_HANDSHAKE:
      pkc_do_handshake(pkc);
      return 0;
#endif
    default:
      bytes = read(pkc->sockfd, PKC_IN(*pkc), PKC_IN_FREE(*pkc));
  }

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
    pk_log(PK_LOG_BE_DATA|PK_LOG_TUNNEL_DATA, "pkc_read() hit EOF");
    pkc->status |= CONN_STATUS_CLS_READ;
  }
  else {
    pk_log(PK_LOG_BE_DATA|PK_LOG_TUNNEL_DATA,
           "pkc_read() error, errno=%d, ssl_errno=%d", errno, ssl_errno);
#ifdef HAVE_OPENSSL
    switch (ssl_errno) {
      case SSL_ERROR_WANT_READ:
      case SSL_ERROR_WANT_WRITE:
        pkc_start_handshake(pkc, ssl_errno);
        break;
      case SSL_ERROR_SYSCALL:
      case SSL_ERROR_NONE:
#endif
        switch (errno) {
          case EINTR:
          case EAGAIN:
            break;
          default:
            pkc->status |= CONN_STATUS_BROKEN;
            break;
        }
#ifdef HAVE_OPENSSL
        break;
      default:
        pkc->status |= CONN_STATUS_BROKEN;
        break;
    }
#endif
  }
  return bytes;
}

ssize_t pkc_raw_write(struct pk_conn* pkc, char* data, ssize_t length) {
  ssize_t wrote = 0;
  switch (pkc->state) {
#ifdef HAVE_OPENSSL
    case CONN_SSL_DATA:
      if (length) {
        wrote = SSL_write(pkc->ssl, data, length);
        if (wrote < 0) {
          int err = SSL_get_error(pkc->ssl, wrote);
          switch (err) {
            case SSL_ERROR_NONE:
              break;
            case SSL_ERROR_WANT_WRITE:
              pk_log(PK_LOG_BE_DATA|PK_LOG_TUNNEL_DATA,
                     "%d: %p/%d/%d/WANT_WRITE", pkc->sockfd, data, wrote, length);
              pkc->status |= CONN_STATUS_WANT_WRITE;
              break;
            default:
              pk_log(PK_LOG_BE_DATA|PK_LOG_TUNNEL_DATA,
                     "%d: SSL_ERROR=%d: %p/%d/%d", pkc->sockfd, err, data, wrote, length);
          }
        }
      }
      break;

    case CONN_SSL_HANDSHAKE:
      pkc_do_handshake(pkc);
      return 0;
#endif

    default:
      if (length)
        wrote = write(pkc->sockfd, data, length);
  }
  return wrote;
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
    wrote = pkc_raw_write(pkc, pkc->out_buffer, pkc->out_buffer_pos);
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
      bytes = pkc_raw_write(pkc, data+wrote, length-wrote);
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

ssize_t pkc_write(struct pk_conn* pkc, char* data, ssize_t length)
{
  ssize_t wleft;
  ssize_t wrote = 0;

  /* 1. Try to flush already buffered data. */
  if (pkc->out_buffer_pos)
    pkc_flush(pkc, NULL, 0, NON_BLOCKING_FLUSH, "pkc_write/1");

  /* 2. If successful, try to write new data (0 copies!) */
  if (0 == pkc->out_buffer_pos) {
    errno = 0;
    do {
      wrote = pkc_raw_write(pkc, data, length);
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
      pkc_flush(pkc, data+wrote, length-wrote, BLOCKING_FLUSH, "pkc_write/2");
    }
  }

  return length;
}
