/******************************************************************************
pkerror.c - Basic error handling utilites

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "pkerror.h"


void* pk_err_null(int error) {
  pk_error = error;
  return NULL;
}

void pk_perror(const char* prefix) {
  if (ERR_ALL_IS_WELL == pk_error) return;

  switch (pk_error) {
    /* FIXME: Make this prettier */
    case ERR_CONNECT_CONNECT:
      perror(prefix);
      fprintf(stderr, "\n");
      break;
    default:
      fprintf(stderr, "%s: pk_error = %d\n", prefix, pk_error);
  }

  pk_error = ERR_ALL_IS_WELL;
}

