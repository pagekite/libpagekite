/******************************************************************************
pkblocker.h - Blocking tasks handled outside the main event loop.

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

struct pk_manager;

typedef enum {
  PK_NO_JOB,
  PK_CHECK_WORLD,
  PK_CHECK_FRONTENDS,
  PK_QUIT
} pk_job_t;

struct pk_job {
  pk_job_t  job;
  void*     data;
};

struct pk_job_pile {
  pthread_mutex_t  mutex;
  pthread_cond_t   cond;
  struct pk_job*   pile; /* A pile is not a queue, order isn't guaranteed. */
  int              max;
  int              count;
};

int   pkb_add_job      (struct pk_job_pile*, pk_job_t, void*);
int   pkb_get_job      (struct pk_job_pile*, struct pk_job*);

int   pkb_start_blocker(struct pk_manager*);
void  pkb_stop_blocker (struct pk_manager*);
