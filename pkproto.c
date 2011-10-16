#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pkproto.h"
#include "utils.h"

void frame_reset_values(struct pk_frame* frame)
{
  frame->length = -1;
  frame->data = NULL;
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

int parser_parse(struct pk_parser *parser, int length, char *data)
{
  struct pk_chunk *chunk = parser->chunk;
  struct pk_frame *frame = &(parser->chunk->frame);
  int zeros;

  if (length > parser->buffer_bytes_left) length = parser->buffer_bytes_left;

  /* Append to buffer */
  memcpy(frame->raw_frame + frame->raw_length, data, length);
  frame->raw_length += length;
  parser->buffer_bytes_left -= length;

  /* Do we have a length? */
  if ((frame->length < 0) && (frame->raw_length > 2))
  {
    /* Convert first CR/LF sequence to \0, to mark the end of the string */
    if (zeros = zero_first_crlf(frame->raw_length, frame->raw_frame))
    {
    }
  }

  return length;
}


/**[ TESTS ]******************************************************************/

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

int pkproto_test_parser(struct pk_parser* p)
{
  int bytes_left = p->buffer_bytes_left;

  parser_parse(p, 5, "12345");
  parser_reset(p);
  parser_parse(p, 5, "54321");

  assert(strncmp(p->chunk->frame.raw_frame, "54321", 5) == 0);
  assert(p->buffer_bytes_left == bytes_left-5);
  assert(p->chunk->frame.raw_length == 5);

  return 1;
}

int pkproto_test(void)
{
  char buffer[64000];
  struct pk_parser* p = parser_create(64000, buffer, NULL);
  return (pkproto_test_alloc(64000, buffer, p) &&
          pkproto_test_parser(p));
}

