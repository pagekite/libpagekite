/******************************************************************************
pkerror.h - Basic error handling utilities

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

#define ERR_ALL_IS_WELL            0

#define ERR_PARSE_BAD_FRAME   -10000
#define ERR_PARSE_BAD_CHUNK   -10001
#define ERR_PARSE_NO_MEMORY   -10002

#define ERR_PARSE_NO_KITENAME -20000
#define ERR_PARSE_NO_BSALT    -20001
#define ERR_PARSE_NO_FSALT    -20002
#define ERR_PARSE_UNSIGNED    -20003
#define ERR_PARSE_BAD_SIG     -20004
#define ERR_PARSE_BAD_FSALT   -20005

#define ERR_CONNECT_LOOKUP    -30000
#define ERR_CONNECT_CONNECT   -30001
#define ERR_CONNECT_REQUEST   -30002
#define ERR_CONNECT_REQ_END   -30003
#define ERR_CONNECT_LISTEN    -30004
#define ERR_CONNECT_ACCEPT    -30005
#define ERR_CONNECT_TLS       -30006

#define ERR_CONNECT_DUPLICATE -40000
#define ERR_CONNECT_REJECTED  -40001

#define ERR_NO_MORE_KITES     -50000
#define ERR_NO_MORE_FRONTENDS -50001
#define ERR_NO_FRONTENDS      -50002
#define ERR_NO_KITE           -50003
#define ERR_RAW_NEEDS_PUBPORT -50004
#define ERR_NO_IPVX           -50005

#define ERR_TOOBIG_MANAGER    -60000
#define ERR_TOOBIG_KITES      -60001
#define ERR_TOOBIG_FRONTENDS  -60002
#define ERR_TOOBIG_BE_CONNS   -60003
#define ERR_TOOBIG_PARSERS    -60004
#define ERR_NO_THREAD         -60005
#define ERR_WSA_STARTUP       -60006


#ifdef __GNUC__
extern __thread int pk_error;
#else
extern int pk_error;
#endif

int    pk_set_error(int);
void*  pk_err_null(int);
void   pk_perror(const char*);       
