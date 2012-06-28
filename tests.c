/******************************************************************************
tests.c - Unit tests for pagekite.

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

Note: For alternate license terms, see the file COPYING.md.

******************************************************************************/
#include <assert.h>
#include <time.h>

#include "pkstate.h"

struct pk_global_state pk_state;

int sha1_test();
int utils_test();
int pkproto_test();
int pkmanager_test();

int main(void) {
  assert(sha1_test() &&
         utils_test() &&
         pkproto_test() &&
         pkmanager_test());
  return 0;
}


