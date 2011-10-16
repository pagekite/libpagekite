#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "pkproto.h"
#include "utils.h"

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
  chunk->request_host = NULL;
  chunk->request_proto = NULL;
  chunk->request_port = NULL;
  chunk->remote_ip = NULL;
  chunk->remote_port = NULL;
  chunk->length = -1;
  chunk->data = NULL;
}

void chunk_reset(struct pk_chunk* chunk)
{
  chunk_reset_values(chunk);
  frame_reset(&(chunk->frame));
}

struct pk_parser* parser_create(int buf_length, char* buf, void* chunk_cb)
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
  parser->buffer_bytes_left = buf_length - parser_size;

  return(parser);
}

void parser_reset(struct pk_parser *parser)
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
      return PARSE_BAD_FRAME;
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
    else if (0 == strncasecmp(frame->data + pos, "EOF: ", 5))
      chunk->eof = frame->data + pos + 5;
    else if (0 == strncasecmp(frame->data + pos, "Host: ", 6))
      chunk->request_host = frame->data + pos + 6;
    else if (0 == strncasecmp(frame->data + pos, "Proto: ", 7))
      chunk->request_proto = frame->data + pos + 7;
    else if (0 == strncasecmp(frame->data + pos, "Port: ", 6))
      chunk->request_port = frame->data + pos + 6;
    else if (0 == strncasecmp(frame->data + pos, "RIP: ", 5))
      chunk->remote_ip = frame->data + pos + 5;
    else if (0 == strncasecmp(frame->data + pos, "RPort: ", 7))
      chunk->remote_port = frame->data + pos + 7;

    pos += len;
  }
  if (2 == len) {
    pos += len;
    chunk->length = frame->length - pos;
    chunk->data = frame->data + pos;
    return pos;
  }
  else return PARSE_BAD_CHUNK;
}

int parser_parse_new_data(struct pk_parser *parser, int length)
{
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
    if (PARSE_BAD_FRAME == parse_frame_header(frame)) return PARSE_BAD_FRAME;
  }
  if (frame->length < 0) return length;

  /* If we have a length, do we have all the data? */
  if (frame->length + frame->hdr_length <= frame->raw_length)
  {
    if (PARSE_BAD_CHUNK == parse_chunk_header(frame, chunk))
      return PARSE_BAD_CHUNK;

    printf("Chunk is complete, we should call the callback\n");
  }

  return length;
}

int parser_parse(struct pk_parser *parser, int length, char *data)
{
  struct pk_frame *frame = &(parser->chunk->frame);
  if (length > parser->buffer_bytes_left) length = parser->buffer_bytes_left;
  memcpy(frame->raw_frame + frame->raw_length, data, length);
  return parser_parse_new_data(parser, length);
}


/**[ TESTS ]******************************************************************/

int pkproto_test_parser(struct pk_parser* p)
{
  char* testchunk = "SID: 1\r\nEOF: r\r\n\r\n54321";
  char buffer[100];
  int length;
  int bytes_left = p->buffer_bytes_left;

  assert(parser_parse(p, 8, "z\r\n12345") == PARSE_BAD_FRAME); parser_reset(p);
  assert(parser_parse(p, 8, "5\r\n54321") == PARSE_BAD_CHUNK); parser_reset(p);

  length = strlen(testchunk);
  length += sprintf(buffer, "%x\r\n", length);
  strcat(buffer, testchunk);

  assert(parser_parse(p, length, buffer) == length);

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
  struct pk_parser* p = parser_create(64000, buffer, NULL);
  return (pkproto_test_alloc(64000, buffer, p) &&
          pkproto_test_parser(p));
}

