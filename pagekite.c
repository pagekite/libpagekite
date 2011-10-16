/******************************************************************************
pagekite.c - A high-performance PageKite implementation in C.

This file is Copyright 2011, The Beanstalks Project ehf.

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

