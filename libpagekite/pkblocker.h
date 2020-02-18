/******************************************************************************
pkblocker.h - Blocking tasks handled outside the main event loop.

This file is Copyright 2011-2020, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify it under
the terms  of the  Apache  License 2.0  as published by the  Apache  Software
Foundation.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the Apache License for more details.

You should have received a copy of the Apache License along with this program.
If not, see: <http://www.apache.org/licenses/>

Note: For alternate license terms, see the file COPYING.md.

******************************************************************************/

struct pk_manager;

typedef enum {
  PK_NO_JOB,
  PK_CHECK_WORLD,
  PK_CHECK_FRONTENDS,
  PK_RELAY_INCOMING,
  PK_QUIT,
} pk_job_t;

struct pk_job {
  PK_MEMORY_CANARY
  pk_job_t  job;
  int       int_data;
  void*     ptr_data;
};

struct pk_job_pile {
  pthread_mutex_t  mutex;
  pthread_cond_t   cond;
  struct pk_job*   pile; /* A pile is not a queue, order isn't guaranteed. */
  int              max;
  int              count;
  PK_MEMORY_CANARY
};

struct pk_blocker {
  pthread_t          thread;
  struct pk_manager* manager;
};

int   pkb_add_job      (struct pk_job_pile*, pk_job_t, int, void*);
int   pkb_get_job      (struct pk_job_pile*, struct pk_job*);

int   pkb_start_blockers(struct pk_manager*, int);
void  pkb_stop_blockers (struct pk_manager*);
