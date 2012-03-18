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
#include "pkerror.h"
#include "pkproto.h"


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
  chunk->request_host = NULL;
  chunk->request_proto = NULL;
  chunk->request_port = -1;
  chunk->remote_ip = NULL;
  chunk->remote_port = -1;
  chunk->remote_tls = NULL;
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
  while (2 < (len = zero_first_crlf(frame->length - pos, frame->data + pos)))
  {
    if (0 == strncasecmp(frame->data + pos, "SID: ", 5))
      chunk->sid = frame->data + pos + 5;
    else if (0 == strncasecmp(frame->data + pos, "NOOP: ", 6))
      chunk->noop = frame->data + pos + 6;
    else if (0 == strncasecmp(frame->data + pos, "Host: ", 6))
      chunk->request_host = frame->data + pos + 6;
    else if (0 == strncasecmp(frame->data + pos, "Proto: ", 7))
      chunk->request_proto = frame->data + pos + 7;
    else if (0 == strncasecmp(frame->data + pos, "Port: ", 6))
      sscanf(frame->data + pos + 6, "%d", &(chunk->request_port));
    else if (0 == strncasecmp(frame->data + pos, "RIP: ", 5))
      chunk->remote_ip = frame->data + pos + 5;
    else if (0 == strncasecmp(frame->data + pos, "RPort: ", 7))
      sscanf(frame->data + pos + 7, "%d", &(chunk->remote_port));
    else if (0 == strncasecmp(frame->data + pos, "RTLS: ", 6))
      chunk->remote_tls = frame->data + pos + 6;
    else if (0 == strncasecmp(frame->data + pos, "EOF: ", 5))
      chunk->eof = frame->data + pos + 5;
    else if (0 == strncasecmp(frame->data + pos, "PING: ", 6))
      chunk->ping = frame->data + pos + 6;
    else if (0 == strncasecmp(frame->data + pos, "SPD: ", 5))
      sscanf(frame->data + pos + 5, "%d", &(chunk->throttle_spd));

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

int pk_format_frame(char* buf, const char* sid, const char *headers, int bytes)
{
  int hlen;
  if (!sid) sid = "";
  hlen = strlen(sid) + strlen(headers) - 2;
  hlen = sprintf(buf, "%x\r\n", hlen + bytes);
  return hlen + sprintf(buf + hlen, headers, sid);
}

int pk_format_reply(char* buf, const char* sid, int bytes, const char* input)
{
  int hlen = pk_format_frame(buf, sid, "SID: %s\r\n\r\n", bytes);
  memcpy(buf + hlen, input, bytes);
  return hlen + bytes;
}

int pk_format_eof(char* buf, const char *sid)
{
  return pk_format_frame(buf, sid, "SID: %s\r\nEOF: rw\r\n\r\n", 0);
}

int pk_format_pong(char* buf)
{
  return pk_format_frame(buf, "", "NOOP: 1%s\r\n\r\n", 0);
}


/**[ Connecting ]**************************************************************/

int pk_make_bsalt(struct pk_kite_request* kite) {
  uint8_t buffer[1024];
  SHA1_CTX context;

  if (kite->bsalt == NULL) kite->bsalt = malloc(41);
  if (kite->bsalt == NULL) {
    fprintf(stderr, "WARNING: Failed to malloc() for bsalt");
    return -1;
  }

  /* FIXME: This is not very random. */
  sprintf((char*) buffer, "%x %x %x %x",
                          rand(), getpid(), getppid(), (int) time(0));

  SHA1_Init(&context);
  SHA1_Update(&context, buffer, strlen((const char*) buffer));
  SHA1_Final(&context, buffer);
  digest_to_hex(buffer, kite->bsalt);
  kite->bsalt[36] = '\0';

  return 1;
}

int pk_sign_kite_request(char *buffer, struct pk_kite_request* kite, int salt) {
  char request[1024];
  char request_s[1024];
  char request_salted_s[1024];
  char proto[64];
  char* fsalt;
  SHA1_CTX context;
  uint8_t signature[64];

  if (kite->bsalt == NULL)
    if (pk_make_bsalt(kite) < 0)
      return 0;

  if (kite->port > 0)
    sprintf(proto, "%s-%d", kite->proto, kite->port);
  else
    strcpy(proto, kite->proto);

  if (kite->fsalt != NULL)
    fsalt = kite->fsalt;
  else
    fsalt = "";

  sprintf(request, "%s:%s:%s:%s", proto, kite->kitename, kite->bsalt, fsalt);
  sprintf(request_salted_s, "%8.8x", salt);
  sprintf(request_s, "%s%s%s", kite->secret, request, request_salted_s);

  SHA1_Init(&context);
  SHA1_Update(&context, (uint8_t*) request_s, strlen(request_s));
  SHA1_Final(&context, signature);
  digest_to_hex(signature, request_salted_s+8);
  request_salted_s[36] = '\0';

  strcat(request, ":");
  strcat(request, request_salted_s);

  return sprintf(buffer, PK_HANDSHAKE_KITE, request);
}

char *pk_parse_kite_request(struct pk_kite_request* kite, const char *line)
{
  char* copy;
  char* p;

  copy = malloc(strlen(line)+1);
  strcpy(copy, line);

  kite->proto = strchr(copy, ' ');
  if (kite->proto == NULL)
    kite->proto = copy;
  else
    kite->proto++;

  if (NULL == (kite->kitename = strchr(kite->proto, ':')))
    return pk_err_null(ERR_PARSE_NO_KITENAME);
  if (NULL == (kite->bsalt = strchr(kite->kitename+1, ':')))
    return pk_err_null(ERR_PARSE_NO_BSALT);
  if (NULL == (kite->fsalt = strchr(kite->bsalt+1, ':')))
    return pk_err_null(ERR_PARSE_NO_FSALT);

  *(kite->kitename++) = '\0';
  *(kite->bsalt++) = '\0';
  *(kite->fsalt++) = '\0';

  if (NULL != (p = strchr(kite->proto, '-'))) {
    *p++ = '\0';
    sscanf(p, "%d", &(kite->port));
  }
  else
    kite->port = 0;

  return copy;
}

int pk_connect(char *frontend, int port, struct sockaddr_in* serv_addr,
               unsigned int n, struct pk_kite_request** kites)
{
  unsigned int i;
  int sockfd, bytes;
  char buffer[16*1024];
  char* p;
  struct pk_kite_request tkite;
  struct sockaddr_in serv_addr_buf;
  struct hostent *server;

  if (serv_addr == NULL)
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

  if ((0 > (sockfd = socket(AF_INET, SOCK_STREAM, 0))) ||
      (0 > connect(sockfd, (struct sockaddr*) serv_addr, sizeof(*serv_addr))) ||
      (0 > write(sockfd, PK_HANDSHAKE_CONNECT, strlen(PK_HANDSHAKE_CONNECT))))
  {
    close(sockfd);
    return (pk_error = ERR_CONNECT_CONNECT);
  }

  for (i = 0; i < n; i++) {
    bytes = pk_sign_kite_request(buffer, kites[i], rand());
    if ((0 >= bytes) || (0 > write(sockfd, buffer, bytes))) {
      close(sockfd);
      return (pk_error = ERR_CONNECT_REQUEST);
    }
  }

  if (0 > write(sockfd, PK_HANDSHAKE_END, strlen(PK_HANDSHAKE_END))) {
    close(sockfd);
    return (pk_error = ERR_CONNECT_REQ_END);
  }

  /* Gather response from server */
  for (i = 0; i < sizeof(buffer)-1; i++) {
    if (1 > read(sockfd, buffer+i, 1)) break;
    if (i > 4) {
      if (0 == strcmp(buffer+i-2, "\n\r\n")) break;
      if (0 == strcmp(buffer+i-1, "\n\n")) break;
    }
  }
  buffer[i] = '\0';

  /* First, if we're rejected, parse out why and bail. */
  p = buffer;
  i = 0;
  while ((p = strstr(p, "X-PageKite-Duplicate")) != NULL) {
    bytes = zero_first_crlf(sizeof(buffer) - (p-buffer), p);
    fprintf(stderr, "Duplicate: %s\n", p);
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
    fprintf(stderr, "Rejected: %s\n", p);
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
    pk_parse_kite_request(&tkite, p);
    for (i = 0; i < n; i++) {
      if ((kites[i]->port == tkite.port) &&
          (0 == strcmp(kites[i]->kitename, tkite.kitename)) &&
          (0 == strcmp(kites[i]->proto, tkite.proto)))
      {
        kites[i]->fsalt = tkite.fsalt;
        i++;
      }
    }
    p += bytes;
  }
  if (i) {
    close(sockfd);
    return pk_connect(frontend, port, serv_addr, n, kites);
  }

  return sockfd;
}
