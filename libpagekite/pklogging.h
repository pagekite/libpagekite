/******************************************************************************
utils.h - Logging.

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

#if defined(PK_TRACE) && (PK_TRACE >= 1)
#define PK_TRACE_FUNCTION pk_log(PK_LOG_TRACE, "trace/%s", __FUNCTION__)
#define PK_TRACE_LOOP(msg) pk_log(PK_LOG_TRACE, "trace/%s: %s", __FUNCTION__, msg)
#else
#define PK_TRACE_FUNCTION
#define PK_TRACE_LOOP(msg)
#endif

extern FILE* PK_DISABLE_LOGGING;

int pk_log(int, const char *fmt, ...);
int pk_log_chunk(struct pk_tunnel*, struct pk_chunk*);
void pk_log_raw_data(int, char*, int, void*, size_t);
void pk_dump_parser(char*, struct pk_parser*);
void pk_dump_conn(char*, struct pk_conn*);
void pk_dump_tunnel(char*, struct pk_tunnel*);
void pk_dump_be_conn(char*, struct pk_backend_conn*);
void pk_dump_state(struct pk_manager*);

