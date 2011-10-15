#include <stdlib.h>
#include "pkproto.h"

void frame_reset(struct s_frame* frame)
{
  frame->length = 0;
  frame->data = NULL;
  frame->raw_length = 0;
  frame->raw_frame = NULL;
}

void chunk_reset(struct s_chunk* chunk)
{
  chunk->sid = NULL;
  chunk->request_host = NULL;
  chunk->request_proto = NULL;
  chunk->request_port = NULL;
  chunk->remote_ip = NULL;
  chunk->remote_port = NULL;
  chunk->length = 0;
  chunk->data = NULL;
  frame_reset(&(chunk->frame));
}

struct s_parser* parser_create(int buf_length, char* buf, void* chunk_cb) {
  struct s_parser* parser;

  parser = (struct s_parser *) buf;
  parser->parser_size = sizeof(struct s_parser);
  parser->parser_size_max = buf_length;
  parser->chunk_callback = chunk_cb;

  parser->current_chunk = (struct s_chunk *) (buf + parser->parser_size);
  parser->parser_size += sizeof(struct s_chunk);

  chunk_reset(parser->current_chunk);

  return(parser);
}

int parser_parse(struct s_parser *parser, int length, char *data) {
  return 0;
}


