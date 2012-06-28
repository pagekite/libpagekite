/******************************************************************************
pkerror.h - Basic error handling utilities

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

#define ERR_ALL_IS_WELL            0

#define ERR_PARSE_BAD_FRAME   -10000
#define ERR_PARSE_BAD_CHUNK   -10001

#define ERR_PARSE_NO_KITENAME -20000
#define ERR_PARSE_NO_BSALT    -20001
#define ERR_PARSE_NO_FSALT    -20002

#define ERR_CONNECT_CONNECT   -30000
#define ERR_CONNECT_REQUEST   -30001
#define ERR_CONNECT_REQ_END   -30002

#define ERR_CONNECT_DUPLICATE -40000
#define ERR_CONNECT_REJECTED  -40001

#define ERR_NO_MORE_KITES     -50000
#define ERR_NO_MORE_FRONTENDS -50001

#define ERR_TOOBIG_MANAGER    -60000
#define ERR_TOOBIG_KITES      -60001
#define ERR_TOOBIG_FRONTENDS  -60002
#define ERR_TOOBIG_BE_CONNS   -60003
#define ERR_TOOBIG_PARSERS    -60004


int    pk_error;
void*  pk_err_null(int);
void   pk_perror(const char*);       
