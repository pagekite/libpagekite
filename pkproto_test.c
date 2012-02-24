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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "pkproto.h"
#include "utils.h"


int pkproto_test_format_frame(void)
{
  char dest[1024];
  char* expect = "e\r\nSID: 12345\r\n\r\n";
  int bytes;
  struct pk_chunk chunk;

  chunk.sid = "12345";
  bytes = strlen(expect);
  assert(bytes == pk_format_frame(dest, &chunk, "SID: %s\r\n\r\n", 0));
  assert(0 == strncmp(expect, dest, bytes));
  return 1;
}

int pkproto_test_format_reply(void)
{
  char dest[1024];
  char* expect = "19\r\nSID: 12345\r\n\r\nHello World";
  int bytes;
  struct pk_chunk chunk;

  chunk.sid = "12345";
  bytes = strlen(expect);
  assert(bytes == pk_format_reply(dest, &chunk, 11, "Hello World"));
  assert(0 == strncmp(expect, dest, bytes));
  return 1;
}

int pkproto_test_format_eof(void)
{
  char dest[1024];
  char* expect = "20\r\nSID: 12345\r\nEOF: RW\r\nNOOP: 1\r\n\r\n";
  int bytes;
  struct pk_chunk chunk;

  chunk.sid = "12345";
  bytes = strlen(expect);
  assert(bytes == pk_format_eof(dest, &chunk));
  assert(0 == strncmp(expect, dest, bytes));
  return 1;
}

int pkproto_test_format_pong(void)
{
  char dest[1024];
  char* expect = "b\r\nNOOP: 1\r\n\r\n";
  int bytes;
  struct pk_chunk chunk;

  chunk.sid = NULL;
  bytes = strlen(expect);
  assert(bytes == pk_format_pong(dest, &chunk));
  assert(0 == strncmp(expect, dest, bytes));
  return 1;
}

int pkproto_test_parser(struct pk_parser* p)
{
  char* testchunk = "SID: 1\r\neOf: r\r\n\r\n54321";
  char buffer[100];
  int length;
  int bytes_left = p->buffer_bytes_left;

  assert(pk_parser_parse(p, 8, "z\r\n12345") == PARSE_BAD_FRAME);
  pk_parser_reset(p);

  assert(pk_parser_parse(p, 8, "5\r\n54321") == PARSE_BAD_CHUNK);
  pk_parser_reset(p);

  length = strlen(testchunk);
  length += sprintf(buffer, "%x\r\n", length);
  strcat(buffer, testchunk);

  assert(pk_parser_parse(p, length, buffer) == length);

  assert(p->buffer_bytes_left == bytes_left - length);
  assert(p->chunk->frame.raw_length == length);
  assert(p->chunk->frame.length == (int) strlen(testchunk));
  assert(p->chunk->frame.data != NULL);
  assert(p->chunk->length == 5);
  assert(p->chunk->data != NULL);

  assert(strncmp(p->chunk->frame.data, "SID", 3) == 0);
  assert(strncmp(p->chunk->sid, "1", 1) == 0);
  assert(strncmp(p->chunk->eof, "r", 1) == 0);
  assert(strncmp(p->chunk->data, "54321", 5) == 0);

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

int pkproto_test(void)
{
  char buffer[64000];
  struct pk_parser* p = pk_parser_init(64000, buffer,
                                       (pkChunkCallback*) NULL, NULL);
  return (pkproto_test_format_frame() &&
          pkproto_test_format_reply() &&
          pkproto_test_format_eof() &&
          pkproto_test_format_pong() &&
          pkproto_test_alloc(64000, buffer, p) &&
          pkproto_test_parser(p));
}

