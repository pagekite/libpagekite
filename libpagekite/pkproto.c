/******************************************************************************
pkproto.c - A basic serializer/deserializer for the PageKite tunnel protocol.

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

#define PAGEKITE_CONSTANTS_ONLY
#include "pagekite.h"

#include "pkcommon.h"
#include "pkutils.h"
#include "pkconn.h"
#include "pkhooks.h"
#include "pkproto.h"
#include "pkstate.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"
#include "pkerror.h"

#ifdef HAVE_OPENSSL
#include <openssl/sha.h>
#else
#include "pd_sha1.h"
#endif

void pk_reset_pagekite(struct pk_pagekite* kite)
{
  PK_ADD_MEMORY_CANARY(kite);
  kite->protocol[0] = '\0';
  kite->public_domain[0] = '\0';
  kite->public_port = 0;
  kite->local_domain[0] = '\0';
  kite->local_port = 0;
  kite->auth_secret[0] = '\0';
}

void frame_reset_values(struct pk_frame* frame)
{
  PK_ADD_MEMORY_CANARY(frame);
  frame->data = NULL;
  frame->length = -1;
  frame->hdr_length = -1;
  frame->raw_length = 0;
}

void frame_reset(struct pk_frame* frame)
{
  frame_reset_values(frame);
  frame->raw_frame = NULL;
}

void pk_chunk_reset_values(struct pk_chunk* chunk)
{
  PK_ADD_MEMORY_CANARY(chunk);
  chunk->sid = NULL;
  chunk->eof = NULL;
  chunk->noop = NULL;
  chunk->ping = NULL;
  chunk->request_proto = NULL;
  chunk->request_host = NULL;
  chunk->request_port = -1;
  chunk->remote_ip = NULL;
  chunk->remote_port = -1;
  chunk->remote_tls = NULL;
  chunk->remote_sent_kb = -1;
  chunk->throttle_spd = -1;
  chunk->quota_days = -1;
  chunk->quota_conns = -1;
  chunk->quota_mb = -1;
  chunk->first_chunk = 0;
  chunk->length = -1;
  chunk->total = -1;
  chunk->offset = 0;
  chunk->data = NULL;
}

void pk_chunk_reset(struct pk_chunk* chunk)
{
  pk_chunk_reset_values(chunk);
  frame_reset(&(chunk->frame));
}

struct pk_parser* pk_parser_init(int buf_length, char* buf,
                                 pkChunkCallback* chunk_cb, void* chunk_cb_data)
{
  int parser_size;
  struct pk_parser* parser;

  parser = (struct pk_parser *) buf;
  parser_size = sizeof(struct pk_parser);

  PK_ADD_MEMORY_CANARY(parser);

  parser->chunk = (struct pk_chunk *) (buf + parser_size);
  parser_size += sizeof(struct pk_chunk);
  pk_chunk_reset(parser->chunk);

  parser->chunk->frame.raw_frame = (char *) (buf + parser_size);

  parser->chunk_callback = chunk_cb;
  parser->chunk_callback_data = chunk_cb_data;
  parser->buffer_bytes_left = buf_length - parser_size;

  PK_CHECK_MEMORY_CANARIES;
  return(parser);
}

void pk_parser_reset(struct pk_parser *parser)
{
  PK_ADD_MEMORY_CANARY(parser);
  parser->buffer_bytes_left += parser->chunk->frame.raw_length;
  frame_reset_values(&(parser->chunk->frame));
  pk_chunk_reset_values(parser->chunk);
}

int parse_frame_header(struct pk_frame* frame)
{
  int hdr_len;
  /* FIXME: Handle ZChunks */
  /* Try to convert first CRLF to \0, to mark the end of the frame header */
  if (0 < (hdr_len = zero_first_crlf(frame->raw_length, frame->raw_frame)))
  {
    frame->hdr_length = hdr_len;
    frame->data = frame->raw_frame + hdr_len;
    if (sizeof(frame->length) == sizeof(long unsigned int)) {
      if (1 != sscanf(frame->raw_frame,
                      "%lx", (long unsigned int *) &(frame->length))) {
        return (pk_error = ERR_PARSE_BAD_FRAME);
      }
    }
    else {
      assert(sizeof(frame->length) == sizeof(unsigned int));
      if (1 != sscanf(frame->raw_frame,
                      "%x", (unsigned int *) &(frame->length))) {
        return (pk_error = ERR_PARSE_BAD_FRAME);
      }
    }
  }
  return 0;
}

int parse_chunk_header(struct pk_frame* frame, struct pk_chunk* chunk,
                       size_t bytes)
{
  int len, pos = 0;
  char first;
  chunk->header_count = 0;
  while (2 < (len = zero_first_crlf(bytes - pos, frame->data + pos)))
  {
    PK_TRACE_LOOP("chunk-header-lines");

    /* This gives us an upper-case (US-ASCII) of the first character. */
    first = *(frame->data + pos) & (0xff - 32);

    /* Cases ordered roughly by frequency, without too much obfuscation. */
    if (first == 'S') {
      if (0 == strncasecmp(frame->data + pos, "SID: ", 5))
        chunk->sid = frame->data + pos + 5;
      else if (0 == strncasecmp(frame->data + pos, "SKB: ", 5)) {
        if (sizeof(chunk->remote_sent_kb) == sizeof(long int)) {
          sscanf(frame->data + pos + 5,
                 "%ld", (long int *) &(chunk->remote_sent_kb));
        }
        else {
          assert(sizeof(chunk->remote_sent_kb) == sizeof(unsigned int));
          sscanf(frame->data + pos + 5,
                 "%d", (int *) &(chunk->remote_sent_kb));
        }
      }
      else if (0 == strncasecmp(frame->data + pos, "SPD: ", 5))
        sscanf(frame->data + pos + 5, "%d", &(chunk->throttle_spd));
    }
    else if (0 == strncasecmp(frame->data + pos, "NOOP: ", 6))
      chunk->noop = frame->data + pos + 6;
    else if (first == 'P') {
      if (0 == strncasecmp(frame->data + pos, "PING: ", 6))
        chunk->ping = frame->data + pos + 6;
      else if (0 == strncasecmp(frame->data + pos, "Proto: ", 7))
        chunk->request_proto = frame->data + pos + 7;
      else if (0 == strncasecmp(frame->data + pos, "Port: ", 6))
        sscanf(frame->data + pos + 6, "%d", &(chunk->request_port));
    }
    else if (0 == strncasecmp(frame->data + pos, "EOF: ", 5))
      chunk->eof = frame->data + pos + 5;
    else if (first == 'R') {
      if (0 == strncasecmp(frame->data + pos, "RIP: ", 5))
        chunk->remote_ip = frame->data + pos + 5;
      else if (0 == strncasecmp(frame->data + pos, "RPort: ", 7))
        sscanf(frame->data + pos + 7, "%d", &(chunk->remote_port));
      else if (0 == strncasecmp(frame->data + pos, "RTLS: ", 6))
        chunk->remote_tls = frame->data + pos + 6;
    }
    else if (0 == strncasecmp(frame->data + pos, "Host: ", 6))
      chunk->request_host = frame->data + pos + 6;
    else if (first == 'Q') {
      if (0 == strncasecmp(frame->data + pos, "QDays: ", 7)) {
        if (1 == sscanf(frame->data + pos + 7, "%d", &(chunk->quota_days)))
          pk_state.quota_days = chunk->quota_days;
      } else if (0 == strncasecmp(frame->data + pos, "QConns: ", 8)) {
        if (1 == sscanf(frame->data + pos + 8, "%d", &(chunk->quota_conns)))
          pk_state.quota_conns = chunk->quota_conns;
      } else if (0 == strncasecmp(frame->data + pos, "Quota: ", 7)) {
        if (1 == sscanf(frame->data + pos + 7, "%d", &(chunk->quota_mb)))
          pk_state.quota_mb = chunk->quota_mb;
      }
    }
    else if (chunk->header_count < PK_MAX_CHUNK_HEADERS) {
      /* Just store pointers to any other headers, for later processing. */
      chunk->headers[chunk->header_count++] = frame->data + pos;
    }

    pos += len;
  }
  if (2 == len) {
    pos += len;
    chunk->total = frame->length - pos;
    chunk->length = bytes - pos;
    chunk->data = frame->data + pos;
    chunk->offset = 0;
    PK_CHECK_MEMORY_CANARIES;
    return pos;
  }
  else {
    PK_CHECK_MEMORY_CANARIES;
    return (pk_error = ERR_PARSE_BAD_CHUNK);
  }
}

int pk_parser_parse_new_data(struct pk_parser *parser, int length)
{
  int leftovers = 0;
  int fragmenting = 0;
  int wanted_length = 0;
  int parse_length = 0;
  struct pk_chunk *chunk = parser->chunk;
  struct pk_frame *frame = &(parser->chunk->frame);

  /* No data, nothing to do. */
  if (length <= 0) return length;

  /* Update counters. */
  frame->raw_length += length;
  parser->buffer_bytes_left -= length;

  /* If we don't have enough data for useful work, finish here. */
  if (frame->raw_length < 3) return length;

  /* Do we have still need to parse the frame header? */
  if (frame->length < 0) {
    if (0 != parse_frame_header(frame))
      return pk_error;
  }
  if (frame->length < 0) return length;

  wanted_length = frame->length + frame->hdr_length;
  parse_length = frame->length;

  /* If the buffer is full, start fragmenting... */
  if ((parser->buffer_bytes_left < 1) &&
       (wanted_length > frame->raw_length)) {
    fragmenting = 1;
    parse_length = frame->raw_length - frame->hdr_length;
    PK_TRACE_LOOP("fragmenting");
  }

  /* Do we have enough data? */
  if (fragmenting ||
      (parser->chunk->data != NULL) ||
      (frame->raw_length >= wanted_length))
  {
    /* Only parse the chunk header once, if we're fragmenting. */
    if (parser->chunk->data == NULL) {
      if (ERR_PARSE_BAD_CHUNK == parse_chunk_header(frame, chunk, parse_length))
        return (pk_error = ERR_PARSE_BAD_CHUNK);
    }
    else {
      if (chunk->offset + length > chunk->total) {
        chunk->length = chunk->total - chunk->offset;
      }
      else {
        chunk->length = length;
      }
    }
    chunk->offset += chunk->length;

    if (parser->chunk_callback != (pkChunkCallback *) NULL) {
      char *eof = chunk->eof;
      char *data = chunk->data;
      size_t length = chunk->length;

      PK_TRACE_LOOP("callback");
      if (fragmenting) chunk->eof = NULL;  /* Suppress EOFs */
      parser->chunk_callback(parser->chunk_callback_data, chunk);

      /* Restore these, as they may have been modified in the callback and
       * thus broken our accounting. */
      chunk->eof = eof;
      chunk->data = data;
      chunk->length = length;
      chunk->first_chunk = 0;
    }

    if (fragmenting || (chunk->offset < chunk->total)) {
      PK_TRACE_LOOP("adjusting");
      frame->length -= chunk->length;
      frame->raw_length -= chunk->length;
      parser->buffer_bytes_left += chunk->length;
    }
    else {
      leftovers = frame->raw_length - wanted_length;
      if (leftovers > 0) {
        PK_TRACE_LOOP("memmove");
        memmove(frame->raw_frame,
                frame->raw_frame + wanted_length,
                leftovers);
        pk_parser_reset(parser);
        pk_parser_parse_new_data(parser, leftovers);
      }
      else {
        PK_TRACE_LOOP("reset");
        pk_parser_reset(parser);
      }
    }
  }

  PK_CHECK_MEMORY_CANARIES;
  return length;
}

int pk_parser_parse(struct pk_parser *parser, int length, char *data)
{
  struct pk_frame *frame = &(parser->chunk->frame);
  int parsed = 0;
  int status = 0;
  int copy = 0;
  do {
    PK_TRACE_LOOP("parsing");

    if ((length > 0) && (0 >= parser->buffer_bytes_left)) {
      /* We will make no progress.  This is bad! */
      return (pk_error = ERR_PARSE_NO_MEMORY);
    }

    if (length > parser->buffer_bytes_left) {
      copy = parser->buffer_bytes_left;
    }
    else {
      copy = length;
    }

    memcpy(frame->raw_frame + frame->raw_length, data, copy);
    status = pk_parser_parse_new_data(parser, copy);
    if (status < 0) {
      pk_parser_reset(parser);
      return status;
    }

    parsed += status;
    length -= status;
    data += status;
  } while (length > 0);

  PK_CHECK_MEMORY_CANARIES;
  return parsed;
}


/**[ Serialization ]**********************************************************/

size_t pk_format_frame(char* buf, const char* sid,
                       const char *headers, size_t bytes)
{
  size_t hlen;
  if (!sid) sid = "";
  hlen = strlen(sid) + strlen(headers) - 2;
  hlen = sprintf(buf, "%zx\r\n", hlen + bytes);
  return hlen + sprintf(buf + hlen, headers, sid);
}

size_t pk_reply_overhead(const char *sid, size_t bytes)
{
  size_t hexlen, chunkhdr;
  chunkhdr = (5 + strlen(sid) + 4); /* SID: %s\r\n\r\n */
  hexlen = 0;
  bytes += chunkhdr;
  do {
    hexlen++;
    bytes = bytes >> 4;
  } while (bytes);
  return hexlen + 2 + chunkhdr; /* %x\r\n... */
}

size_t pk_format_reply(char* buf, const char* sid,
                       size_t bytes, const char* input)
{
  size_t hlen;
  hlen = pk_format_frame(buf, sid, "SID: %s\r\n\r\n", bytes);
  if (NULL != input) {
    memcpy(buf + hlen, input, bytes);
    return hlen + bytes;
  }
  else
    return hlen;
}

ssize_t pk_format_chunk(char* buf, size_t maxbytes, struct pk_chunk* chunk)
{
  size_t bytes = 0;
  size_t hlen = pk_reply_overhead(chunk->sid, maxbytes);
  char* tbuf = malloc(maxbytes - hlen);
  if (tbuf == NULL) return (pk_error = ERR_PARSE_NO_MEMORY);

  #define free_and_return(rv) { free(tbuf); return rv; }
  #define _add_str(h, v) if (v != NULL) { \
                  if (hlen + bytes + strlen(h) + strlen(v) + 5 <= maxbytes) { \
                    bytes += sprintf(tbuf + bytes, "%s: %s\r\n", h, v); \
                  } else free_and_return(pk_error = ERR_PARSE_NO_MEMORY); }
  #define _add_int(h, v) if (v >= 0) { \
                  if (hlen + bytes + strlen(h) + 10 + 5 <= maxbytes) { \
                    bytes += sprintf(tbuf + bytes, "%s: %d\r\n", h, v); \
                  } else free_and_return(pk_error = ERR_PARSE_NO_MEMORY); }
  #define _add_sst(h, v) if (v >= 0) { \
                  if (hlen + bytes + strlen(h) + 10 + 5 <= maxbytes) { \
                    bytes += sprintf(tbuf + bytes, "%s: %ld\r\n", h, v); \
                  } else free_and_return(pk_error = ERR_PARSE_NO_MEMORY); }

  _add_str("EOF",    chunk->eof);
  _add_str("NOOP",   chunk->noop);
  _add_str("PING",   chunk->ping);
  _add_str("Host",   chunk->request_host);
  _add_int("Port",   chunk->request_port);
  _add_str("Proto",  chunk->request_proto);
  _add_str("RIP",    chunk->remote_ip);
  _add_int("RPort",  chunk->remote_port);
  _add_str("RTLS",   chunk->remote_tls);
  _add_sst("SKB",    chunk->remote_sent_kb);
  _add_int("QDays",  chunk->quota_days);
  _add_int("QConns", chunk->quota_conns);
  _add_int("Quota",  chunk->quota_mb);

  bytes += sprintf(tbuf + bytes, "\r\n");

  if (chunk->length > 0) {
    if (hlen + chunk->length + bytes - 2 > maxbytes) {
      free_and_return(pk_error = ERR_PARSE_NO_MEMORY);
    }
    memcpy(tbuf + bytes, chunk->data, chunk->length);
    bytes += chunk->length;
  }

  hlen = pk_format_frame(buf, chunk->sid, "SID: %s\r\n", bytes);
  memcpy(buf + hlen, tbuf, bytes);

  free_and_return(hlen + bytes);
}

size_t pk_format_eof(char* buf, const char* sid, int how)
{
  char format[64];
  sprintf(format, "SID: %%s\r\nEOF: 1%s%s\r\n\r\n",
                  (how & PK_EOF_READ) ? "R" : "",
                  (how & PK_EOF_WRITE) ? "W" : "");
  return pk_format_frame(buf, sid, format, 0);
}

size_t pk_format_skb(char* buf, const char* sid, int kilobytes)
{
  char format[64];
  sprintf(format, "NOOP: 1\r\nSID: %%s\r\nSKB: %d\r\n\r\n", kilobytes);
  return pk_format_frame(buf, sid, format, 0);
}

size_t pk_format_pong(char* buf)
{
  return pk_format_frame(buf, "", "NOOP: 1%s\r\n\r\n", 0);
}

size_t pk_format_ping(char* buf)
{
  return pk_format_frame(buf, "", "NOOP: 1%s\r\nPING: 1\r\n\r\n", 0);
}

size_t pk_format_http_rejection(
  char* buf,
  int frontend_sockfd,
  const char* default_fancy_url,
  const char* request_proto,
  const char* request_host)
{
  char pre[PK_REJECT_MAXSIZE];
  char relay_ip[128];
  char* fancy_url = NULL;
  char* post = NULL;

  if (default_fancy_url && *default_fancy_url) {
    struct pke_event* ev = pke_post_blocking_event(NULL,
      PK_EV_CFG_FANCY_URL, 0, request_host, NULL, &fancy_url);
    if (NULL == fancy_url) fancy_url = (char*) default_fancy_url;

    relay_ip[0] = '\0';
    if (frontend_sockfd != PK_REJECT_BACKEND) {
      struct sockaddr_in sin;
      socklen_t len = sizeof(sin);
      if (-1 != getsockname(frontend_sockfd, (struct sockaddr*) &sin, &len))
        in_ipaddr_to_str((struct sockaddr*) &sin, relay_ip, 128);
    }

    sprintf(pre, PK_REJECT_FANCY_PRE,
                 fancy_url,
                 (frontend_sockfd == PK_REJECT_BACKEND) ? "BE" : "FE",
                 pk_state.app_id_short,
                 request_proto, request_host, relay_ip);
    post = PK_REJECT_FANCY_POST;

    if (ev) pke_free_event(NULL, ev->event_code);
    if (fancy_url != default_fancy_url) free(fancy_url);
  }
  else {
    pre[0] = '\0';
    post = pre;
  }

  return sprintf(buf, PK_REJECT_FMT,
                      pre,
                      (frontend_sockfd == PK_REJECT_BACKEND) ? "be" : "fe",
                      pk_state.app_id_short,
                      request_proto, request_host, post);
}




/**[ Connecting ]**************************************************************/

int pk_make_salt(char* salt) {
  uint8_t buffer[1024];
  char digest[41];

  PK_TRACE_FUNCTION;

  int slen = sprintf((char*) buffer, "%x %x", rand(), (int) time(0));

#ifdef HAVE_OPENSSL
  SHA_CTX context;
  SHA1_Init(&context);
  SHA1_Update(&context, (uint8_t*) random_junk, 31);
  SHA1_Update(&context, buffer, slen);
  SHA1_Final((unsigned char*) buffer, &context);
#else
  PD_SHA1_CTX context;
  pd_sha1_init(&context);
  pd_sha1_update(&context, (uint8_t*) random_junk, 31);
  pd_sha1_update(&context, buffer, slen);
  pd_sha1_final(&context, buffer);
#endif
  digest_to_hex(buffer, digest);
  strncpyz(salt, digest, PK_SALT_LENGTH);

  return 1;
}

char* pk_sign(const char* token, const char* secret, time_t ts,
              const char* payload, int length, char *buffer)
{
  char tbuffer[128], tsbuf[16], scratch[10240];

  PK_TRACE_FUNCTION;

  if (token == NULL) {
    sprintf(scratch, "%8.8x", rand());
#ifdef HAVE_OPENSSL
    SHA_CTX context;
    SHA1_Init(&context);
    SHA1_Update(&context, (uint8_t*) secret, strlen(secret));
    SHA1_Update(&context, (uint8_t*) random_junk, 31);
    SHA1_Update(&context, (uint8_t*) scratch, 8);
    SHA1_Final((unsigned char*) scratch, &context);
#else
    PD_SHA1_CTX context;
    pd_sha1_init(&context);
    pd_sha1_update(&context, (uint8_t*) secret, strlen(secret));
    pd_sha1_update(&context, (uint8_t*) random_junk, 31);
    pd_sha1_update(&context, (uint8_t*) scratch, 8);
    pd_sha1_final(&context, scratch);
#endif
    digest_to_hex((uint8_t*) scratch, tbuffer);
    token = tbuffer;
  }
  strncpy(buffer, token, 8);
  buffer[8] = '\0';

  /* Optionally embed a timestamp to the resolution of 10 minutes */
  if (ts > 0) {
    sprintf(tsbuf, "%lx", ts / 600);
    buffer[0] = 't';
  }
  else tsbuf[0] = '\0';

#ifdef HAVE_OPENSSL
  SHA_CTX context;
  SHA1_Init(&context);
  SHA1_Update(&context, (uint8_t*) secret, strlen(secret));
  if (payload)
    SHA1_Update(&context, (uint8_t*) payload, strlen(payload));
  SHA1_Update(&context, (uint8_t*) tsbuf, strlen(tsbuf));
  SHA1_Update(&context, (uint8_t*) buffer, 8);
  SHA1_Final((unsigned char*)scratch, &context);
#else
  PD_SHA1_CTX context;
  pd_sha1_init(&context);
  pd_sha1_update(&context, (uint8_t*) secret, strlen(secret));
  if (payload)
    pd_sha1_update(&context, (uint8_t*) payload, strlen(payload));
  pd_sha1_update(&context, (uint8_t*) tsbuf, strlen(tsbuf));
  pd_sha1_update(&context, (uint8_t*) buffer, 8);
  pd_sha1_final(&context, scratch);
#endif
  digest_to_hex((uint8_t*) scratch, buffer+8);
  buffer[length] = '\0';
  return buffer;
}

char *pk_prepare_kite_challenge(char* buffer, struct pk_kite_request* kite_r,
                                char* secret, time_t ts) {
  char proto[64];
  struct pk_pagekite* kite;

  PK_TRACE_FUNCTION;

  kite = kite_r->kite;
  if (kite->public_port > 0)
    sprintf(proto, "%s-%d", kite->protocol, kite->public_port);
  else
    strcpy(proto, kite->protocol);

  if (secret != NULL) {
    char* npe = buffer;
    npe += sprintf(buffer, "%s:%s:%s", proto, kite->public_domain,
                                       kite_r->bsalt);
    pk_sign(kite_r->fsalt[0] ? kite_r->fsalt : NULL, secret, ts,
            buffer, PK_SALT_LENGTH, npe + 1);
    *npe = ':';
  }
  else {
    sprintf(buffer, "%s:%s:%s:%s", proto, kite->public_domain,
                                   kite_r->bsalt, kite_r->fsalt);
  }

  return buffer;
}

int pk_sign_kite_request(char *buffer, struct pk_kite_request* kite_r, int salt) {
  char request[1024];
  char request_sign[1024];
  char request_salt[1024];
  struct pk_pagekite* kite;

  PK_TRACE_FUNCTION;

  kite = kite_r->kite;
  if (kite_r->bsalt[0] == '\0')
    if (pk_make_salt(kite_r->bsalt) < 0)
      return 0;

  pk_prepare_kite_challenge(request, kite_r, NULL, 0);

  sprintf(request_salt, "%8.8x", salt);
  pk_sign(request_salt, kite->auth_secret, 0, request, 36, request_sign);

  strcat(request, ":");
  strcat(request, request_sign);

  return sprintf(buffer, PK_HANDSHAKE_KITE, request);
}

char *pk_parse_kite_request(
  struct pk_kite_request* kite_r,
  char** signature,
  const char *line)
{
  char* copy;
  char* p;
  char* public_domain;
  char* bsalt;
  char* fsalt;
  char* bsig;
  char* protocol;
  struct pk_pagekite* kite = kite_r->kite;

  PK_TRACE_FUNCTION;

  int llen = strlen(line);
  copy = malloc(llen+1);
  strcpy(copy, line);

  char* end = copy + llen;
  *end = '\0';

  /* Parse the string... */
  protocol = strchr(copy, ' ');
  if (protocol == NULL) {
    protocol = copy;
  }
  else {
    protocol++;
  }
  if (NULL != (public_domain = strchr(protocol, ':'))) {
    *(public_domain++) = '\0';
    if ((public_domain < end) && NULL != (bsalt = strchr(public_domain, ':'))) {
      *(bsalt++) = '\0';
      if ((bsalt < end) && NULL != (fsalt = strchr(bsalt, ':'))) {
        *(fsalt++) = '\0';
        if ((fsalt < end) && NULL != (bsig = strchr(fsalt, ':'))) {
          *(bsig++) = '\0';
        }
        else bsig = "";
      }
      else fsalt = bsig = "";
    }
    else bsalt = fsalt = bsig = "";
  }
  else public_domain = bsalt = fsalt = bsig = "";

  /* Error out if things are too large. */
  if ((strlen(protocol) > PK_PROTOCOL_LENGTH) ||
      (strlen(public_domain) > PK_DOMAIN_LENGTH) ||
      (strlen(bsalt) > PK_SALT_LENGTH) ||
      (strlen(fsalt) > PK_SALT_LENGTH)) {
    free(copy);
    return pk_err_null(ERR_PARSE_NO_KITENAME);
  }

  /* Copy the values */
  strncpyz(kite->protocol, protocol, PK_PROTOCOL_LENGTH);
  strncpyz(kite->public_domain, public_domain, PK_DOMAIN_LENGTH);
  strncpyz(kite_r->bsalt, bsalt, PK_SALT_LENGTH);
  strncpyz(kite_r->fsalt, fsalt, PK_SALT_LENGTH);
  if (NULL != (p = strchr(kite->protocol, '-'))) {
    *p++ = '\0';
    sscanf(p, "%d", &(kite->public_port));
  }
  else
    kite->public_port = 0;
  if (*bsig && signature) {
    *signature = strdup(bsig);
  }
  else if (signature)
    *signature = NULL;

  /* Cleanup */
  free(copy);

  /* Pass judgement */
  if ('\0' == *public_domain) return pk_err_null(ERR_PARSE_NO_KITENAME);
  if ('\0' == *bsalt) return pk_err_null(ERR_PARSE_NO_BSALT);
  if ('\0' == *fsalt) return pk_err_null(ERR_PARSE_NO_FSALT);
  return kite->public_domain;
}

struct pk_kite_request* pk_parse_pagekite_response(
  char* buffer,
  size_t bufsize,
  char* session_id,
  char* motd)
{
  PK_TRACE_FUNCTION;

  /* Walk through the buffer and count how many responses we have.
   * This will overshoot a bit, but that should be harmless. */
  int responses = 1;
  char* p = buffer;
  char lastc = buffer[bufsize - 1];
  buffer[bufsize - 1] = '\0'; /* Ensure strcasestr will terminate */
  while (NULL != (p = strcasestr(p, "X-PageKite-"))) {
    responses++;
    p++;
  }
  buffer[bufsize - 1] = lastc; /* Put things back the way they were */

  /* FIXME: Magic number. This prevents malicious remotes from making us
   *        allocate arbitrary amounts of RAM, but also puts an undocumented
   *        upper limit on how many kites we support per tunnel. */
  if (responses >= 1000) return pk_err_null(ERR_NO_MORE_KITES);

  /* Allocate space for pk_kite_request and pk_pagekite structures.
   * We put the pk_kite_requests first, and then tack the pk_pagekites on
   * to the end. The following math calculates first how much size we need
   * for the pk_kite_request structures, and then recalculate in terms of
   * pk_pagekite equivalents. These shenanigans allow us to get away with
   * a single malloc() and easier cleanup elsewhere. */
  int rsize = responses * sizeof(struct pk_kite_request);
  int rskip = 1 + (rsize / sizeof(struct pk_pagekite)); /* +1 because / rounds down */
  rsize = (rskip + responses) * sizeof(struct pk_pagekite);

  struct pk_kite_request* res = malloc(rsize);
  struct pk_kite_request* rp = res;
  struct pk_pagekite* rpk = ((struct pk_pagekite*) res) + rskip;
  if (res == NULL) return pk_err_null(ERR_NO_MORE_KITES);

  /* Set the pk_pagekite pointer for each allocated pk_kite_request */
  for (int i = 0; i < responses; i++) res[i].kite = &(rpk[i]);

  /* Walk through the response header line-by-line and parse.
   * This loop is destructive due to use of zero_first_eol.
   */
  int bytes;
  p = buffer;
  do {
    PK_TRACE_LOOP("response line");

    /* Note: We use zero_first_eol instead of zero_first_crlf, to be
     *       more tolerant of slightly lame implementations. */
    bytes = zero_first_eol(bufsize - (p-buffer), p);
    rp->status = PK_KITE_UNKNOWN;

                     /* 12345678901 = 11 bytes */
    if (strncasecmp(p, "X-PageKite-", 11) == 0)
    {
                          /* 123 = 3 bytes */
      if (strncasecmp(p+11, "OK:", 3) == 0) {
        rp->status = PK_KITE_FLYING;
      }
                               /* 1234567 = 7 bytes */
      else if (strncasecmp(p+11, "SSL-OK:", 7) == 0) {
        /* FIXME: Note that we are OK with relay MITM SSL active! */
        /* FIXME: Backtrack rp to avoid duplicate entries response? */
      }
                               /* 1234567890 = 10 bytes */
      else if (strncasecmp(p+11, "Duplicate:", 10) == 0) {
        rp->status = PK_KITE_DUPLICATE;
      }
                               /* 12345678 = 8 bytes */
      else if (strncasecmp(p+11, "Invalid:", 8) == 0) {
        rp->status = PK_KITE_INVALID;
      }
                               /* 123456789012 = 12 bytes */
      else if (strncasecmp(p+11, "Invalid-Why:", 12) == 0) {
        /* FIXME: Parse the reason out of the response */
        /* FIXME: Backtrack rp to avoid duplicate entries response? */
        pk_log(PK_LOG_TUNNEL_DATA, "Why: %s", p+11+13);
      }
                               /* 123456789 = 9 bytes */
      else if (strncasecmp(p+11, "SignThis:", 9) == 0) {
        rp->status = PK_KITE_WANTSIG;
      }
                               /* 123456 = 6 bytes */
      else if (strncasecmp(p+11, "Quota:", 6) == 0) {
        /* FIXME: Parse out current bandwidth quota value */
      }
                               /* 1234567 = 7 bytes */
      else if (strncasecmp(p+11, "QConns:", 7) == 0) {
        /* FIXME: Parse out current connection quota value */
      }
                               /* 123456 = 6 bytes */
      else if (strncasecmp(p+11, "QDays:", 6) == 0) {
        /* FIXME: Parse out current days-left quota value */
      }
      else if (session_id &&    /* 1234567890 = 10 bytes */
               (strncasecmp(p+11, "SessionID:", 10) == 0)) {
        strncpyz(session_id, p+11+10+1, PK_HANDSHAKE_SESSIONID_MAX);
        pk_log(PK_LOG_TUNNEL_DATA, "Session ID is: %s", session_id);
      }
      else if (motd &&          /* 12345 = 5 bytes */
               (strncasecmp(p+11, "Misc:", 5) == 0)) {
        /* FIXME: Record/log the "misc" messages. */
      }

      if (rp->status != PK_KITE_UNKNOWN) {
        if ((NULL != pk_parse_kite_request(rp, NULL, p)) ||
            (rp->status != PK_KITE_FLYING)) {
          rp++;
        }
      }
    }

    p += bytes;
  } while (bytes);

  rp->status = PK_KITE_UNKNOWN;
  return res;
}


int pk_connect_ai(struct pk_conn* pkc, struct addrinfo* ai, int reconnecting,
                  unsigned int n, struct pk_kite_request* requests,
                  char *session_id, SSL_CTX *ctx, const char* hostname)
{
  unsigned int i, j, bytes;
  char buffer[16*1024], *p;
  struct pk_pagekite tkite;
  struct pk_kite_request tkite_r;

  PK_TRACE_FUNCTION;

  pkc->status |= CONN_STATUS_CHANGING;
  pk_log(PK_LOG_TUNNEL_CONNS,
         "Connecting to %s (session=%s%s%s)",
         in_addr_to_str(ai->ai_addr, buffer, 1024),
         (session_id && session_id[0] != '\0') ? session_id : "new",
         (pkc->status & FE_STATUS_IS_FAST) ? ", is fast" : "",
         (pkc->status & FE_STATUS_IN_DNS) ? ", in DNS" : "",
         (pkc->status & FE_STATUS_NAILED_UP) ? ", nailed up" : "");
  if (0 > pkc_connect(pkc, ai))
    return (pk_error = ERR_CONNECT_CONNECT);

  set_blocking(pkc->sockfd);

#ifdef HAVE_OPENSSL
  if ((ctx != NULL) &&
      (0 != pkc_start_ssl(pkc, ctx, hostname)))
    return (pk_error = ERR_CONNECT_TLS);
#endif

  memset(&buffer, 0, 16*1024);
  pkc_write(pkc, PK_HANDSHAKE_CONNECT, strlen(PK_HANDSHAKE_CONNECT));
  pkc_write(pkc, PK_HANDSHAKE_FEATURES, strlen(PK_HANDSHAKE_FEATURES));
  if (session_id && *session_id) {
    pk_log(PK_LOG_TUNNEL_DATA, " - Session ID: %s", session_id);
    sprintf(buffer, PK_HANDSHAKE_SESSION, session_id);
    pkc_write(pkc, buffer, strlen(buffer));
  }

  for (i = 0; i < n; i++) {
    if (requests[i].kite->protocol[0] != '\0') {
      requests[i].status = PK_KITE_UNKNOWN;
      bytes = pk_sign_kite_request(buffer, &(requests[i]), rand());
      pk_log(PK_LOG_TUNNEL_DATA, " * %s", buffer);
      pkc_write(pkc, buffer, bytes);
    }
  }

  pk_log(PK_LOG_TUNNEL_DATA, " - End handshake, flushing.");
  pkc_write(pkc, PK_HANDSHAKE_END, strlen(PK_HANDSHAKE_END));
  if (0 > pkc_flush(pkc, NULL, 0, BLOCKING_FLUSH, "pk_connect_ai")) {
    pkc_reset_conn(pkc, CONN_STATUS_CHANGING|CONN_STATUS_ALLOCATED);
    return (pk_error = ERR_CONNECT_REQUEST);
  }

  /* Gather response from server */
  pk_log(PK_LOG_TUNNEL_DATA, " - Read response ...");
  for (i = 0; i < sizeof(buffer)-1 &&
#ifdef HAVE_OPENSSL
              (pkc->state != CONN_SSL_HANDSHAKE) &&
#endif
              !(pkc->status & (CONN_STATUS_BROKEN|CONN_STATUS_CLS_READ)); )
  {
    PK_TRACE_LOOP("read response");
    if (1 > pkc_wait(pkc, 2000)) return (pk_error = ERR_CONNECT_REQUEST);
    pk_log(PK_LOG_TUNNEL_DATA, " - Have data ...");
    pkc_read(pkc);
    if (pkc->in_buffer_pos > 0) {
      memcpy(buffer+i, pkc->in_buffer, pkc->in_buffer_pos);

      i += pkc->in_buffer_pos;
      pkc->in_buffer_pos = 0;
      buffer[i] = '\0';

      if (i > 4) {
        if (0 == strcmp(buffer+i-3, "\n\r\n")) break;
        if (0 == strcmp(buffer+i-2, "\n\n")) break;
      }
      pk_log(PK_LOG_TUNNEL_DATA, " - Partial buffer: %s", buffer);
    }
  }
  pk_log(PK_LOG_TUNNEL_DATA, " - Parsing!");

  i = 0;
  struct pk_kite_request* rkites = NULL;
  rkites = pk_parse_pagekite_response(buffer, sizeof(buffer), session_id, NULL);

  if (NULL != rkites) {
    struct pk_kite_request* pkr;
    for (pkr = rkites; pkr->status != PK_KITE_UNKNOWN; pkr++) {
      switch (pkr->status) {
        case PK_KITE_REJECTED:
        case PK_KITE_DUPLICATE:
        case PK_KITE_INVALID:
          pkc_reset_conn(pkc, CONN_STATUS_CHANGING|CONN_STATUS_ALLOCATED);
          free(rkites);
          return (pk_error = (
            (pkr->status == PK_KITE_DUPLICATE) ? ERR_CONNECT_DUPLICATE
                                               : ERR_CONNECT_REJECTED));
        case PK_KITE_WANTSIG:
          for (j = 0; j < n; j++) {
            if ((requests[j].kite->protocol[0] != '\0') &&
                (requests[j].kite->public_port == pkr->kite->public_port) &&
                (0 == strcmp(requests[j].kite->public_domain, pkr->kite->public_domain)) &&
                (0 == strcmp(requests[j].kite->protocol, pkr->kite->protocol)))
            {
              pk_log(PK_LOG_TUNNEL_DATA, " - Matched: %s:%s",
                                         requests[j].kite->protocol,
                                         requests[j].kite->public_domain);
              strncpyz(requests[j].fsalt, pkr->fsalt, PK_SALT_LENGTH);
              i++;
            }
          }
          break;
      }
    }
    free(rkites);
  }
  else {
    pkc_reset_conn(pkc, CONN_STATUS_CHANGING|CONN_STATUS_ALLOCATED);
    pk_log(PK_LOG_TUNNEL_DATA, "No response parsed, treating as rejection.");
    return (pk_error = ERR_CONNECT_REJECTED);
  }

  if (i) {
    if (reconnecting) {
      pkc_reset_conn(pkc, CONN_STATUS_CHANGING|CONN_STATUS_ALLOCATED);
      return (pk_error = ERR_CONNECT_REJECTED);
    }
    else {
      pkc_reset_conn(pkc, CONN_STATUS_CHANGING|CONN_STATUS_ALLOCATED);
      return pk_connect_ai(pkc, ai, 1, n, requests, session_id, ctx, hostname);
    }
  }

  for (i = 0; i < n; i++) {
    requests[i].status = PK_KITE_FLYING;
  }
  pk_log(PK_LOG_TUNNEL_DATA, "pk_connect_ai(%s, %d, %p) => %d",
                             in_addr_to_str(ai->ai_addr, buffer, 1024),
                             n, requests, pkc->sockfd);
  return 1;
}

int pk_connect(struct pk_conn* pkc, char *frontend, int port,
               unsigned int n, struct pk_kite_request* requests,
               char *session_id, SSL_CTX *ctx)
{
  int rv;
  char ports[16];
  struct addrinfo hints, *result, *rp;

  PK_TRACE_FUNCTION;

  pkc->status |= CONN_STATUS_CHANGING;
  pk_log(PK_LOG_TUNNEL_CONNS, "pk_connect(%s:%d, %d, %p)",
                              frontend, port, n, requests);

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = AI_ADDRCONFIG;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  sprintf(ports, "%d", port);
  if (0 == (rv = getaddrinfo(frontend, ports, &hints, &result))) {
    for (rp = result; rp != NULL; rp = rp->ai_next) {
      rv = pk_connect_ai(pkc, rp, 0, n, requests, session_id, ctx, frontend);
      if ((rv >= 0) ||
          (rv != ERR_CONNECT_CONNECT)) {
        freeaddrinfo(result);
        return rv;
      }
    }
    freeaddrinfo(result);
  }
  else {
    pk_log(PK_LOG_TUNNEL_CONNS|PK_LOG_ERROR,
           "pk_connect: getaddrinfo(%s, %s) failed:",
           frontend, ports, gai_strerror(rv));
    return (pk_error = ERR_CONNECT_LOOKUP);
  }
  return (pk_error = ERR_CONNECT_CONNECT);
}

int pk_http_forwarding_headers_hook(struct pk_chunk* chunk,
                                    struct pk_backend_conn* pkb)
{
  static unsigned char rewrite_space[PARSER_BYTES_MAX + 256];
  char forwarding_headers[1024];

  PK_TRACE_FUNCTION;

  if (chunk->first_chunk &&
      chunk->request_proto &&
      chunk->remote_ip &&
      ((0 == strcasecmp(chunk->request_proto, "http")) ||
       (0 == strcasecmp(chunk->request_proto, "websocket"))) &&
      (strlen(chunk->remote_ip) < 128) &&
      (chunk->length < PARSER_BYTES_MAX))
  {
      int added = 0;
      char *s = chunk->data;
      char *d = rewrite_space;
      char *nl = "\n";
      int countdown = chunk->length;

      /* This ensures that d-2 is safe in the loop below. */
      if (countdown--) *d++ = *s++;

      while (countdown-- > 0) {
        *d++ = *s++;
        if (!added && (*(d-1) == '\n')) {
          if (*(d-2) == '\r') nl = "\r\n";
          added = sprintf(d,
                          "X-Forwarded-Proto: %s%sX-Forwarded-For: %s%s",
                          chunk->remote_tls ? "https" : "http", nl,
                          chunk->remote_ip, nl);
          d += added;
        }
      }

      if (added) {
        chunk->length += added;
        chunk->data = rewrite_space;
      }
  }

  return 0;
}


/* *** Tests *************************************************************** */

#if PK_TESTS
static int pkproto_test_format_frame(void)
{
  char dest[1024];
  char* expect = "e\r\nSID: 12345\r\n\r\n";
  size_t bytes = strlen(expect);
  assert(bytes == pk_format_frame(dest, "12345", "SID: %s\r\n\r\n", 0));
  assert(0 == strncmp(expect, dest, bytes));
  return 1;
}

static int pkproto_test_format_reply(void)
{
  char dest[1024];
  char* expect = "19\r\nSID: 12345\r\n\r\nHello World";
  size_t bytes = strlen(expect);
  assert(bytes == 11+pk_reply_overhead("12345", 11));
  assert(bytes == pk_format_reply(dest, "12345", 11, "Hello World"));
  assert(0 == strncmp(expect, dest, bytes));
  return 1;
}

static int pkproto_test_format_chunk(void)
{
  struct pk_chunk chunk;
  char dest[1024];

  pk_chunk_reset_values(&chunk);
  chunk.sid = "1234";
  chunk.eof = "1W";
  chunk.quota_mb = 10;
  chunk.data = "Potato salads";
  chunk.length = strlen(chunk.data);
  char* expect = "2e\r\nSID: 1234\r\nEOF: 1W\r\nQuota: 10\r\n\r\nPotato salads";
  size_t bytes = strlen(expect);

  /* Make sure our memory bounds are respected */
  assert(ERR_PARSE_NO_MEMORY == pk_format_chunk(dest, bytes - 1, &chunk));
  assert(bytes == pk_format_chunk(dest, bytes, &chunk));

  assert(0 == strncmp(expect, dest, bytes));
  return 1;
}

static int pkproto_test_format_eof(void)
{
  char dest[1024];
  char* expect = "17\r\nSID: 12345\r\nEOF: 1W\r\n\r\n";
  size_t bytes = strlen(expect);
  assert(bytes == pk_format_eof(dest, "12345", PK_EOF_WRITE));
  assert(0 == strncmp(expect, dest, bytes));
  return 1;
}

static int pkproto_test_format_pong(void)
{
  char dest[1024];
  char* expect = "b\r\nNOOP: 1\r\n\r\n";
  size_t bytes = strlen(expect);
  assert(bytes == pk_format_pong(dest));
  assert(0 == strncmp(expect, dest, bytes));
  return 1;
}

static void pkproto_test_callback(int *data, struct pk_chunk *chunk) {
  assert(chunk->sid != NULL);
  assert(chunk->noop != NULL);
  assert(chunk->data != NULL);
  assert(chunk->frame.data != NULL);
  assert(chunk->remote_ip != NULL);
  assert(chunk->request_proto != NULL);
  assert(0 == strcmp(chunk->sid, "1"));
  assert(0 == strcmp(chunk->noop, "!"));
  assert(-1 == chunk->quota_conns);
  assert(55 == chunk->quota_days);
  assert(1234 == chunk->quota_mb);
  if (*data < 2) {
    assert(chunk->eof != NULL);
    assert(0 == strcmp(chunk->eof, "r"));
    assert(chunk->length == 5);
    assert(0 == strncmp(chunk->data, "54321", chunk->length));
  }
  else if (*data == 2) {
    chunk->first_chunk = 1;
    pk_http_forwarding_headers_hook(chunk, NULL);
    assert(NULL != memmem(chunk->data, chunk->length, "X-Forward", 9));
  }
  else {
    assert(NULL == memmem(chunk->data, chunk->length, "X-Forward", 9));
  }
  *data += 1;
}

static int pkproto_test_parser(struct pk_parser* p, int *callback_called)
{
  char* testchunk = ("SID: 1\r\n"
                     "eOf: r\r\n"
                     "NOOP: !\r\n"
                     "Proto: http\r\n"
                     "RIP: 127.0.0.1\r\n"
                     "Quota: 1234\r\n"
                     "QDays: 55\r\n"
                     "\r\n"
                     "54321");
  char buffer[1024], framehead[10];
  int length;
  int bytes_left = p->buffer_bytes_left;

  assert(pk_parser_parse(p, 8, "z\r\n12345") == ERR_PARSE_BAD_FRAME);
  pk_parser_reset(p);

  assert(pk_parser_parse(p, 8, "5\r\n54321") == ERR_PARSE_BAD_CHUNK);
  pk_parser_reset(p);

  length = strlen(testchunk);
  length += sprintf(buffer, "%x\r\n", length);
  strcpy(framehead, buffer);
  strcat(buffer, testchunk);
  strcat(buffer, framehead);
  strcat(buffer, testchunk);

  assert(2*length == (int) strlen(buffer));
  assert(pk_parser_parse(p, 2*length-10, buffer) == 2*length-10);
  assert(pk_parser_parse(p, 10, buffer+2*length-10) == 10);

  /* After parsing, the callback should have been called twice, all
   * buffer space released for use and the chunk been reset. */
  assert(*callback_called == 2);
  assert(p->buffer_bytes_left == bytes_left);
  assert(p->chunk->data == NULL);
  assert(p->chunk->quota_days == -1);
  assert(p->chunk->remote_ip == NULL);

  assert(55 == pk_state.quota_days);
  assert(1234 == pk_state.quota_mb);

  /* Construct an over-large frame chunk that
   * will require fragmented processing. */
  char *frame = malloc(2 * PARSER_BYTES_MAX);
  int cl = 2 * PARSER_BYTES_MAX - 10;
  int ch = sprintf(frame, "%x\r\n", cl);
  int cs = ch + cl;
  strcat(frame, testchunk);
  int hl = strlen(frame) - 5;
  for (int i = hl; i < ch + cl; i++) frame[i] = 'A' + ((i-hl) % 26);
  memcpy(frame+hl, "GET / HTTP/1.1\r\nHost: foo.bar.baz\r\n\r\n", 38);

  pk_parser_reset(p);
  pk_parser_parse(p, cs, frame);
  free(frame);

  return 1;
}

static int pkproto_test_alloc(unsigned int buf_len, char *buffer,
                              struct pk_parser* p)
{
  unsigned int p_offs, c_offs, f_offs;

  p_offs = ((char*) p) - buffer;
  c_offs = ((char*) p->chunk) - buffer;
  f_offs = ((char*) p->chunk->frame.raw_frame) - buffer;

  if ((p_offs != 0) ||
      (p_offs + sizeof(struct pk_parser) != c_offs) ||
      (c_offs + sizeof(struct pk_chunk) != f_offs) ||
      (f_offs + p->buffer_bytes_left != buf_len))
  {
    printf("Offsets within buffer:\n");
    printf(" Parser     %2.2u (size: %zu)\n", p_offs, sizeof(struct pk_parser));
    printf(" Chunk      %2.2u (size: %zu)\n", c_offs, sizeof(struct pk_chunk));
    printf(" Frame data %2.2u\n", f_offs);
    printf(" Space left %2d/%u\n", p->buffer_bytes_left, buf_len);
    return 0;
  }
  return 1;
}

static int pkproto_test_make_bsalt(void) {
  struct pk_kite_request kite;
  pk_make_salt(kite.bsalt);
  assert(strlen(kite.bsalt) == 36);
  return 1;
}

static int pkproto_test_sign_kite_request(void) {
  struct pk_pagekite kite;
  struct pk_kite_request kite_r;
  unsigned int bytes;
  char* expected = "X-PageKite: http-99:testkite.com:123456789012345678901234567890123456::0000000166483a7e7d92338f838f6e166509\r\n";
  char buffer[120];

  kite_r.kite = &kite;
  strcpy(kite.public_domain, "testkite.com");
  kite.public_port = 99;
  strcpy(kite.auth_secret, "wigglybop");
  strcpy(kite.protocol, "http");
  strcpy(kite_r.bsalt, "123456789012345678901234567890123456");
  strcpy(kite_r.fsalt, "");

  bytes = pk_sign_kite_request(buffer, &kite_r, 1);
  assert(bytes == strlen(expected));
  assert(0 == strcmp(buffer, expected));

  return 1;
}

static int pkproto_test_parse_kite_request(void) {
  struct pk_pagekite kite;
  struct pk_kite_request kite_r;
  char *signature;

  kite_r.kite = &kite;
  pk_parse_kite_request(&kite_r, &signature, "X-PageKite: http-99:b.com:abacab::sig");
  assert(99 == kite.public_port);
  assert(0 == strcmp(kite.public_domain, "b.com"));
  assert(0 == strcmp(kite.protocol, "http"));
  assert(0 == strcmp(kite_r.bsalt, "abacab"));
  assert(0 == strcmp(kite_r.fsalt, ""));
  assert(signature != NULL);
  assert(0 == strcmp(signature, "sig"));
  free(signature);

  return 1;
}
#endif

int pkproto_test(void)
{
#if PK_TESTS
  char buffer[64000];
  int callback_called = 0;

  PK_INIT_MEMORY_CANARIES;

  struct pk_parser* p = pk_parser_init(PARSER_BYTES_MIN, buffer,
                                       (pkChunkCallback*) &pkproto_test_callback,
                                       &callback_called);
  return (pkproto_test_format_frame() &&
          pkproto_test_format_reply() &&
          pkproto_test_format_chunk() &&
          pkproto_test_format_eof() &&
          pkproto_test_format_pong() &&
          pkproto_test_alloc(PARSER_BYTES_MIN, buffer, p) &&
          pkproto_test_parser(p, &callback_called) &&
          pkproto_test_make_bsalt() &&
          pkproto_test_sign_kite_request() &&
          pkproto_test_parse_kite_request());
#else
  return 1;
#endif
}
