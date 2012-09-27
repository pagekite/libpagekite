/******************************************************************************
pkproto.c - A basic serializer/deserializer for the PageKite tunnel protocol.

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
#include "pkerror.h"
#include "pkconn.h"
#include "pkproto.h"
#include "utils.h"


int pkproto_test_format_frame(void)
{
  char dest[1024];
  char* expect = "e\r\nSID: 12345\r\n\r\n";
  size_t bytes = strlen(expect);
  assert(bytes == pk_format_frame(dest, "12345", "SID: %s\r\n\r\n", 0));
  assert(0 == strncmp(expect, dest, bytes));
  return 1;
}

int pkproto_test_format_reply(void)
{
  char dest[1024];
  char* expect = "19\r\nSID: 12345\r\n\r\nHello World";
  size_t bytes = strlen(expect);
  assert(bytes == 11+pk_reply_overhead("12345", 11));
  assert(bytes == pk_format_reply(dest, "12345", 11, "Hello World"));
  assert(0 == strncmp(expect, dest, bytes));
  return 1;
}

int pkproto_test_format_eof(void)
{
  char dest[1024];
  char* expect = "17\r\nSID: 12345\r\nEOF: 1W\r\n\r\n";
  size_t bytes = strlen(expect);
  assert(bytes == pk_format_eof(dest, "12345", PK_EOF_WRITE));
  assert(0 == strncmp(expect, dest, bytes));
  return 1;
}

int pkproto_test_format_pong(void)
{
  char dest[1024];
  char* expect = "b\r\nNOOP: 1\r\n\r\n";
  size_t bytes = strlen(expect);
  assert(bytes == pk_format_pong(dest));
  assert(0 == strncmp(expect, dest, bytes));
  return 1;
}

void pkproto_test_callback(int *data, struct pk_chunk *chunk) {
  assert(chunk->sid != NULL);
  assert(chunk->eof != NULL);
  assert(chunk->noop != NULL);
  assert(chunk->data != NULL);
  assert(chunk->length == 5);
  assert(chunk->frame.data != NULL);
  assert(0 == strcmp(chunk->sid, "1"));
  assert(0 == strcmp(chunk->eof, "r"));
  assert(0 == strcmp(chunk->noop, "!"));
  assert(0 == strncmp(chunk->data, "54321", chunk->length));
  *data += 1;
}

int pkproto_test_parser(struct pk_parser* p, int *callback_called)
{
  char* testchunk = "SID: 1\r\neOf: r\r\nNOOP: !\r\n\r\n54321";
  char buffer[100], framehead[10];
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

  return 1;
}

int pkproto_test_alloc(unsigned int buf_len, char *buffer, struct pk_parser* p)
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
    printf(" Parser     %2.2d (size: %d)\n", p_offs, sizeof(struct pk_parser));
    printf(" Chunk      %2.2d (size: %d)\n", c_offs, sizeof(struct pk_chunk));
    printf(" Frame data %2.2d\n", f_offs);
    printf(" Space left %2d/%d\n", p->buffer_bytes_left, buf_len);
    return 0;
  }
  return 1;
}

int pkproto_test_make_bsalt(void) {
  struct pk_kite_request kite;
  kite.bsalt = NULL;
  pk_make_bsalt(&kite);
  assert(strlen(kite.bsalt) == 36);
  return 1;
}

int pkproto_test_sign_kite_request(void) {
  struct pk_pagekite kite;
  struct pk_kite_request kite_r;
  unsigned int bytes;
  char* expected = "X-PageKite: http-99:testkite.com:123456789012345678901234567890123456::0000000166483a7e7d92338f838f6e166509\r\n";
  char buffer[120];

  kite_r.kite = &kite;
  kite.public_domain = "testkite.com";
  kite.public_port = 99;
  kite.auth_secret = "wigglybop";
  kite.protocol = "http";
  kite_r.bsalt = "123456789012345678901234567890123456";
  kite_r.fsalt = NULL;

  bytes = pk_sign_kite_request(buffer, &kite_r, 1);
  assert(bytes == strlen(expected));
  assert(0 == strcmp(buffer, expected));

  return 1;
}

int pkproto_test_parse_kite_request(void) {
  struct pk_pagekite kite;
  struct pk_kite_request kite_r;

  kite_r.kite = &kite;
  pk_parse_kite_request(&kite_r, "foo: http-99:b.com:abacab:");
  assert(99 == kite.public_port);
  assert(0 == strcmp(kite.public_domain, "b.com"));
  assert(0 == strcmp(kite.protocol, "http"));
  assert(0 == strcmp(kite_r.bsalt, "abacab"));
  assert(0 == strcmp(kite_r.fsalt, ""));

  return 1;
}

int pkproto_test(void)
{
  char buffer[64000];
  int callback_called = 0;
  struct pk_parser* p = pk_parser_init(64000, buffer,
                                       (pkChunkCallback*) &pkproto_test_callback,
                                       &callback_called);
  return (pkproto_test_format_frame() &&
          pkproto_test_format_reply() &&
          pkproto_test_format_eof() &&
          pkproto_test_format_pong() &&
          pkproto_test_alloc(64000, buffer, p) &&
          pkproto_test_parser(p, &callback_called) &&
          pkproto_test_make_bsalt() &&
          pkproto_test_sign_kite_request() &&
          pkproto_test_parse_kite_request());
}

