/******************************************************************************
pkproto.c - A basic serializer/deserializer for the PageKite tunnel protocol.

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

******************************************************************************/
#include <assert.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "types.h"
#include "sha1.h"
#include "utils.h"
#include "pkproto.h"
#include "pklogging.h"
#include "pkerror.h"

void pk_reset_pagekite(struct pk_pagekite* kite)
{
  kite->protocol = NULL;
  kite->public_domain = NULL;
  kite->public_port = 0;
  kite->local_domain = NULL;
  kite->local_port = 0;
  kite->auth_secret = NULL;
}

void frame_reset_values(struct pk_frame* frame)
{
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

void chunk_reset_values(struct pk_chunk* chunk)
{
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
  chunk->length = -1;
  chunk->data = NULL;
}

void chunk_reset(struct pk_chunk* chunk)
{
  chunk_reset_values(chunk);
  frame_reset(&(chunk->frame));
}

struct pk_parser* pk_parser_init(int buf_length, char* buf,
                                 pkChunkCallback* chunk_cb, void* chunk_cb_data)
{
  int parser_size;
  struct pk_parser* parser;

  parser = (struct pk_parser *) buf;
  parser_size = sizeof(struct pk_parser);

  parser->chunk = (struct pk_chunk *) (buf + parser_size);
  parser_size += sizeof(struct pk_chunk);
  chunk_reset(parser->chunk);

  parser->chunk->frame.raw_frame = (char *) (buf + parser_size);

  parser->chunk_callback = chunk_cb;
  parser->chunk_callback_data = chunk_cb_data;
  parser->buffer_bytes_left = buf_length - parser_size;

  return(parser);
}

void pk_parser_reset(struct pk_parser *parser)
{
  parser->buffer_bytes_left += parser->chunk->frame.raw_length;
  frame_reset_values(&(parser->chunk->frame));
  chunk_reset_values(parser->chunk);
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
    if (1 != sscanf(frame->raw_frame, "%x", (unsigned int*) &(frame->length)))
      return (pk_error = ERR_PARSE_BAD_FRAME);
  }
  return 0;
}

int parse_chunk_header(struct pk_frame* frame, struct pk_chunk* chunk)
{
  int len, pos = 0;
  char first;
  chunk->header_count = 0;
  while (2 < (len = zero_first_crlf(frame->length - pos, frame->data + pos)))
  {
    /* This gives us an upper-case (US-ASCII) of the first character. */
    first = *(frame->data + pos) & (0xff - 32);

    /* Cases ordered roughly by frequency, without too much obfuscation. */
    if (first == 'S') {
      if (0 == strncasecmp(frame->data + pos, "SID: ", 5))
        chunk->sid = frame->data + pos + 5;
      else if (0 == strncasecmp(frame->data + pos, "SKB: ", 5))
        sscanf(frame->data + pos + 5, "%d", &(chunk->remote_sent_kb));
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
    else if (chunk->header_count < PK_MAX_CHUNK_HEADERS) {
      /* Just store pointers to any other headers, for later processing. */
      chunk->headers[chunk->header_count++] = frame->data + pos;
    }

    pos += len;
  }
  if (2 == len) {
    pos += len;
    chunk->length = frame->length - pos;
    chunk->data = frame->data + pos;
    return pos;
  }
  else return (pk_error = ERR_PARSE_BAD_CHUNK);
}

int pk_parser_parse_new_data(struct pk_parser *parser, int length)
{
  int leftovers = 0;
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
    if (ERR_PARSE_BAD_FRAME == parse_frame_header(frame))
      return ERR_PARSE_BAD_FRAME;
  }
  if (frame->length < 0) return length;

  /* If we have a length, do we have all the data? */
  if (frame->length + frame->hdr_length <= frame->raw_length)
  {
    if (ERR_PARSE_BAD_CHUNK == parse_chunk_header(frame, chunk))
      return (pk_error = ERR_PARSE_BAD_CHUNK);

    if (parser->chunk_callback != (pkChunkCallback *) NULL)
      parser->chunk_callback(parser->chunk_callback_data, parser->chunk);

    leftovers = frame->raw_length - (frame->length + frame->hdr_length);
    if (leftovers > 0) {
      memmove(frame->raw_frame,
              frame->raw_frame + (frame->length + frame->hdr_length),
              leftovers);
      pk_parser_reset(parser);
      pk_parser_parse_new_data(parser, leftovers);
    }
    else {
      pk_parser_reset(parser);
    }
  }

  return length;
}

int pk_parser_parse(struct pk_parser *parser, int length, char *data)
{
  struct pk_frame *frame = &(parser->chunk->frame);
  int parsed = 0;
  int status = 0;
  int copy = 0;
  do {
    if (length > parser->buffer_bytes_left)
      copy = parser->buffer_bytes_left;
    else
      copy = length;

    memcpy(frame->raw_frame + frame->raw_length, data, copy);
    status = pk_parser_parse_new_data(parser, copy);
    if (status < 0) {
      pk_parser_reset(parser);
      return status;
    }

    parsed += status;
    length -= status;
    data += status;
  } while ((parser->buffer_bytes_left > 0) && (length > 0));
  return parsed;
}


/**[ Serialization ]**********************************************************/

size_t pk_format_frame(char* buf, const char* sid,
                       const char *headers, size_t bytes)
{
  size_t hlen;
  if (!sid) sid = "";
  hlen = strlen(sid) + strlen(headers) - 2;
  hlen = sprintf(buf, "%x\r\n", hlen + bytes);
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

size_t pk_format_reply(char* buf, const char* sid, size_t bytes, const char* input)
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

size_t pk_format_eof(char* buf, const char *sid, int how)
{
  char format[64];
  sprintf(format, "SID: %%s\r\nEOF: 1%s%s\r\n\r\n",
                  (how & PK_EOF_READ) ? "R" : "",
                  (how & PK_EOF_WRITE) ? "W" : "");
  return pk_format_frame(buf, sid, format, 0);
}

size_t pk_format_pong(char* buf)
{
  return pk_format_frame(buf, "", "NOOP: 1%s\r\n\r\n", 0);
}


/**[ Connecting ]**************************************************************/

int pk_make_bsalt(struct pk_kite_request* kite_r) {
  uint8_t buffer[1024];
  SHA1_CTX context;

  if (kite_r->bsalt == NULL) kite_r->bsalt = malloc(41);
  if (kite_r->bsalt == NULL) {
    pk_log(PK_LOG_TUNNEL_CONNS, "WARNING: Failed to malloc() for bsalt");
    return -1;
  }

  /* FIXME: This is not very random. */
  sprintf((char*) buffer, "%x %x %x %x",
                          rand(), getpid(), getppid(), (int) time(0));

  SHA1_Init(&context);
  SHA1_Update(&context, buffer, strlen((const char*) buffer));
  SHA1_Final(&context, buffer);
  digest_to_hex(buffer, kite_r->bsalt);
  kite_r->bsalt[36] = '\0';

  return 1;
}

int pk_sign_kite_request(char *buffer, struct pk_kite_request* kite_r, int salt) {
  char request[1024];
  char request_s[1024];
  char request_salted_s[1024];
  char proto[64];
  char* fsalt;
  SHA1_CTX context;
  uint8_t signature[64];
  struct pk_pagekite* kite;

  kite = kite_r->kite;
  if (kite_r->bsalt == NULL)
    if (pk_make_bsalt(kite_r) < 0)
      return 0;

  if (kite->public_port > 0)
    sprintf(proto, "%s-%d", kite->protocol, kite->public_port);
  else
    strcpy(proto, kite->protocol);

  if (kite_r->fsalt != NULL)
    fsalt = kite_r->fsalt;
  else
    fsalt = "";

  sprintf(request, "%s:%s:%s:%s", proto, kite->public_domain, kite_r->bsalt, fsalt);
  sprintf(request_salted_s, "%8.8x", salt);
  sprintf(request_s, "%s%s%s", kite->auth_secret, request, request_salted_s);

  SHA1_Init(&context);
  SHA1_Update(&context, (uint8_t*) request_s, strlen(request_s));
  SHA1_Final(&context, signature);
  digest_to_hex(signature, request_salted_s+8);
  request_salted_s[36] = '\0';

  strcat(request, ":");
  strcat(request, request_salted_s);

  return sprintf(buffer, PK_HANDSHAKE_KITE, request);
}

char *pk_parse_kite_request(struct pk_kite_request* kite_r, const char *line)
{
  char* copy;
  char* p;
  struct pk_pagekite* kite = kite_r->kite;

  copy = malloc(strlen(line)+1);
  strcpy(copy, line);

  kite->protocol = strchr(copy, ' ');
  if (kite->protocol == NULL)
    kite->protocol = copy;
  else
    kite->protocol++;

  if (NULL == (kite->public_domain = strchr(kite->protocol, ':')))
    return pk_err_null(ERR_PARSE_NO_KITENAME);
  if (NULL == (kite_r->bsalt = strchr(kite->public_domain+1, ':')))
    return pk_err_null(ERR_PARSE_NO_BSALT);
  if (NULL == (kite_r->fsalt = strchr(kite_r->bsalt+1, ':')))
    return pk_err_null(ERR_PARSE_NO_FSALT);

  *(kite->public_domain++) = '\0';
  *(kite_r->bsalt++) = '\0';
  *(kite_r->fsalt++) = '\0';

  if (NULL != (p = strchr(kite->protocol, '-'))) {
    *p++ = '\0';
    sscanf(p, "%d", &(kite->public_port));
  }
  else
    kite->public_port = 0;

  return copy;
}

int pk_connect(char *frontend, int port, struct sockaddr_in* serv_addr,
               unsigned int n, struct pk_kite_request* requests)
{
  unsigned int i, j, reconnecting;
  int sockfd, bytes;
  char buffer[16*1024];
  char* p;
  struct pk_pagekite tkite;
  struct pk_kite_request tkite_r;
  struct sockaddr_in serv_addr_buf;
  struct hostent *server;

  pk_log(PK_LOG_TUNNEL_DATA, "pk_connect(%s:%d, %p, %d, %p)",
                             frontend, port, serv_addr, n, requests);

  reconnecting = (serv_addr != NULL);
  if (!reconnecting)
  {
    if (NULL == (server = gethostbyname(frontend))) return -1;

    serv_addr = &serv_addr_buf;
    bzero((char *) serv_addr, sizeof(serv_addr));
    serv_addr->sin_family = AF_INET;
    bcopy((char*) server->h_addr_list[0],
          (char*) &(serv_addr->sin_addr.s_addr),
          server->h_length);
    serv_addr->sin_port = htons(port);
  }

  pk_log(PK_LOG_TUNNEL_DATA, "socket/connect/write");
  if ((0 > (sockfd = socket(AF_INET, SOCK_STREAM, 0))) ||
      (0 > connect(sockfd, (struct sockaddr*) serv_addr, sizeof(*serv_addr))) ||
      (0 > write(sockfd, PK_HANDSHAKE_CONNECT, strlen(PK_HANDSHAKE_CONNECT))))
  {
    if (sockfd >= 0)
      close(sockfd);
    return (pk_error = ERR_CONNECT_CONNECT);
  }

  for (i = 0; i < n; i++) {
    if (requests[i].kite->protocol != NULL) {
      requests[i].status = PK_STATUS_UNKNOWN;
      bytes = pk_sign_kite_request(buffer, &(requests[i]), rand());
      pk_log(PK_LOG_TUNNEL_DATA, "request(%s)", requests[i].kite->public_domain);
      if ((0 >= bytes) || (0 > write(sockfd, buffer, bytes))) {
        close(sockfd);
        return (pk_error = ERR_CONNECT_REQUEST);
      }
    }
  }

  pk_log(PK_LOG_TUNNEL_DATA, "end handshake");
  if (0 > write(sockfd, PK_HANDSHAKE_END, strlen(PK_HANDSHAKE_END))) {
    close(sockfd);
    return (pk_error = ERR_CONNECT_REQ_END);
  }

  /* Gather response from server */
  pk_log(PK_LOG_TUNNEL_DATA, "read response...");
  for (i = 0; i < sizeof(buffer)-1; i++) {
    if (1 > timed_read(sockfd, buffer+i, 1, 2000)) break;
    if (i > 4) {
      if (0 == strcmp(buffer+i-2, "\n\r\n")) break;
      if (0 == strcmp(buffer+i-1, "\n\n")) break;
    }
  }
  buffer[i] = '\0';

  /* First, if we're rejected, parse out why and bail. */
  /* FIXME: Should update the status of each individual request. */
  p = buffer;
  i = 0;
  while ((p = strstr(p, "X-PageKite-Duplicate")) != NULL) {
    bytes = zero_first_crlf(sizeof(buffer) - (p-buffer), p);
    pk_log(PK_LOG_TUNNEL_CONNS, "Duplicate: %s", p);
    p += bytes;
    i++;
  }
  if (i) {
    close(sockfd);
    return (pk_error = ERR_CONNECT_DUPLICATE);
  }
  p = buffer;
  i = 0;
  while ((p = strstr(p, "X-PageKite-Invalid")) != NULL) {
    bytes = zero_first_crlf(sizeof(buffer) - (p-buffer), p);
    pk_log(PK_LOG_TUNNEL_CONNS, "Rejected: %s", p);
    p += bytes;
    i++;
  }
  if (i) {
    close(sockfd);
    return (pk_error = ERR_CONNECT_REJECTED);
  }

  /* Second, if we need to reconnect with a signature, gather up all the fsalt
   * values and then retry. */
  p = buffer;
  i = 0;
  while ((p = strstr(p, "X-PageKite-SignThis")) != NULL) {
    bytes = zero_first_crlf(sizeof(buffer) - (p-buffer), p);
    tkite_r.kite = &tkite;
    pk_parse_kite_request(&tkite_r, p);
    for (j = 0; j < n; j++) {
      if ((requests[j].kite->protocol != NULL) &&
          (requests[j].kite->public_port == tkite.public_port) &&
          (0 == strcmp(requests[j].kite->public_domain, tkite.public_domain)) &&
          (0 == strcmp(requests[j].kite->protocol, tkite.protocol)))
      {
        requests[j].fsalt = tkite_r.fsalt;
        i++;
      }
    }
    p += bytes;
  }
  if (i) {
    if (reconnecting) {
      close(sockfd);
      return (pk_error = ERR_CONNECT_REJECTED);
    }
    else {
      close(sockfd);
      return pk_connect(frontend, port, serv_addr, n, requests);
    }
  }

  for (i = 0; i < n; i++) {
    requests[i].status = PK_STATUS_CONNECTED;
  }
  pk_log(PK_LOG_TUNNEL_DATA, "pk_connect(%s:%d, %p, %d, %p) => %d",
                             frontend, port, serv_addr, n, requests, sockfd);
  return sockfd;
}
