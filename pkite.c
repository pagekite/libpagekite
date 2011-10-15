#include <stdio.h>
#include <stdlib.h>
#include "pkproto.h"

int main(int argc, char **argv) {
  struct s_parser *p;
  char buffer[64*1024];

  p = parser_create(sizeof(buffer), buffer, NULL);

  printf("Hello world: %d\n", (int) p);
  return(1);
}
