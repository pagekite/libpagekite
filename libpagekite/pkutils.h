/******************************************************************************
pkutils.h - Utility functions for pagekite.

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

#define max(a, b) ((a > b) ? (a) : (b))
#define min(a, b) ((a < b) ? (a) : (b))
#define abs(a)   ((a < 0) ? (-a) : (a))
#define strncpyz(dest, src, len) { strncpy(dest, src, len); dest[len] = '\0'; }
#define free_outside(ptr, buf, len) { if ((ptr != NULL) && ((ptr < buf) || (ptr >= buf+len))) free(ptr); }

#define PK_RANDOM_DEFAULT      -1
#define PK_RANDOM_FIXED_SRAND   0
#define PK_RANDOM_RESEED_SRAND  1

extern char random_junk[];

void better_srand(int);
int32_t murmur3_32(const uint8_t* key, size_t len);
int zero_first_eol(int, char*);
int zero_first_crlf(int, char*);
int zero_first_whitespace(int, char*);
int zero_nth_char(int, char, int, char*);
int strcaseindex(char**, const char*, int);
char* skip_http_header(int, const char*);
char* collapse_whitespace(char*);
int dbg_write(int, char *, int);
int set_non_blocking(int);
int set_blocking(int);
void sleep_ms(int);
time_t pk_time();
void pk_gettime(struct timespec*);
void pk_pthread_condattr_setclock(pthread_condattr_t*);
int wait_fd(int, int);
ssize_t timed_read(int, void*, size_t, int);
struct addrinfo* copy_addrinfo_data(struct addrinfo* dst, struct addrinfo* src);
void free_addrinfo_data(struct addrinfo* dst);
char* in_ipaddr_to_str(const struct sockaddr*, char*, size_t);
char* in_addr_to_str(const struct sockaddr*, char*, size_t);
int addrcmp(const struct sockaddr *, const struct sockaddr *);
int http_get(const char*, char*, size_t);
void digest_to_hex(const unsigned char* digest, char *output);
int printable_binary(char*, size_t, const char*, size_t);
void pk_rlock_init(pk_rlock_t*);
void pk_rlock_lock(pk_rlock_t*);
void pk_rlock_unlock(pk_rlock_t*);

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
