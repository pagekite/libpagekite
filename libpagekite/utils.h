/******************************************************************************
utils.h - Utility functions for pagekite.

This file is Copyright 2011-2014, The Beanstalks Project ehf.

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

#define strncpyz(dest, src, len) { strncpy(dest, src, len); dest[len] = '\0'; }

int zero_first_crlf(int, char*);
char *skip_http_header(int, const char*);
int dbg_write(int, char *, int);
int set_non_blocking(int);
int set_blocking(int);
int wait_fd(int, int);
ssize_t timed_read(int, void*, size_t, int);
char *in_ipaddr_to_str(const struct sockaddr*, char*, size_t);
char *in_addr_to_str(const struct sockaddr*, char*, size_t);
int addrcmp(const struct sockaddr *, const struct sockaddr *);
int http_get(const char*, char*, size_t);
void digest_to_hex(const unsigned char* digest, char *output);

#if PK_MEMORY_CANARIES
# define PK_MEMORY_CANARY           void* canary;
# define PK_INIT_MEMORY_CANARIES    init_memory_canaries();
# define PK_RESET_MEMORY_CANARIES   reset_memory_canaries();
# define PK_CHECK_MEMORY_CANARIES   assert(0 == check_memory_canaries());
# define PK_ADD_MEMORY_CANARY(s)    add_memory_canary(&((s)->canary));
#else
# define PK_MEMORY_CANARY         /* No canaries */
# define PK_INIT_MEMORY_CANARIES  /* No canaries */
# define PK_RESET_MEMORY_CANARIES /* No canaries */
# define PK_CHECK_MEMORY_CANARIES /* No canaries */
# define PK_ADD_MEMORY_CANARY(s)  /* No canaries */
#endif
void add_memory_canary(void**);
void remove_memory_canary(void**);
int check_memory_canaries();
void reset_memory_canaries();
void init_memory_canaries();

int utils_test(void);
