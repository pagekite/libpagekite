/******************************************************************************
pkerror.c - Basic error handling utilites

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

#define PAGEKITE_CONSTANTS_ONLY
#include "pagekite.h"

#include "pkcommon.h"
#include "pkutils.h"
#include "pkerror.h"
#include "pkhooks.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pkstate.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"

#ifdef __GNUC__
__thread int pk_error;
#else
#warning Uh, oh: pk_error is not thread safe on this platform!
int pk_error;
#endif


int pk_set_error(int error) {
  return (pk_error = error);
}

void* pk_err_null(int error) {
  pk_error = error;
  return NULL;
}

void pk_perror(const char* prefix) {
  if (ERR_ALL_IS_WELL == pk_error) return;

  switch (pk_error) {
    case ERR_PARSE_BAD_FRAME:
    case ERR_PARSE_BAD_CHUNK:
    case ERR_PARSE_NO_MEMORY:
    case ERR_PARSE_NO_KITENAME:
    case ERR_PARSE_NO_BSALT:
    case ERR_PARSE_NO_FSALT:
      pk_log(PK_LOG_ERROR, "%s: Internal protocol error %d", prefix, pk_error);
      break;
    case ERR_CONNECT_CONNECT:
      pk_log(PK_LOG_ERROR, "%s: %s", prefix, strerror(errno));
      break;
    case ERR_CONNECT_DUPLICATE:
      pk_log(PK_LOG_ERROR, "%s: Rejected as a duplicate by front-end", prefix);
      break;
    case ERR_CONNECT_REJECTED:
      pk_log(PK_LOG_ERROR, "%s: Rejected by front-end", prefix);
      break;
    case ERR_CONNECT_LOOKUP:
    case ERR_CONNECT_REQUEST:
    case ERR_CONNECT_REQ_END:
      pk_log(PK_LOG_ERROR, "%s: Connection error %d", prefix, pk_error);
      break;
    case ERR_CONNECT_TLS:  /* FIXME: Why did it fail? */
      pk_log(PK_LOG_ERROR, "%s: TLS handshake failed", prefix);
      break;
    case ERR_NO_MORE_KITES:
      pk_log(PK_LOG_ERROR, "%s: Out of kite slots", prefix);
      break;
    case ERR_RAW_NEEDS_PUBPORT:
      pk_log(PK_LOG_ERROR, "%s: Raw kites must specify a public port", prefix);
      break;
    case ERR_NO_MORE_FRONTENDS:
      pk_log(PK_LOG_ERROR, "%s: Out of frontend slots", prefix);
      break;
    case ERR_NO_KITE:
      pk_log(PK_LOG_ERROR, "%s: No kites configured!", prefix);
      break;
    case ERR_NO_FRONTENDS:
      pk_log(PK_LOG_ERROR, "%s: No frontends configured!", prefix);
      break;
    case ERR_TOOBIG_MANAGER:
    case ERR_TOOBIG_KITES:
    case ERR_TOOBIG_FRONTENDS:
    case ERR_TOOBIG_BE_CONNS:
    case ERR_TOOBIG_PARSERS:
      pk_log(PK_LOG_ERROR, "%s: Insufficient allocated memory (%d)",
                           prefix, pk_error);
      break;
    case ERR_NO_IPVX:
      pk_log(PK_LOG_ERROR, "%s: Both IPv6 and IPv4 are disabled!", prefix);
      break;
    default:
      pk_log(PK_LOG_ERROR, "%s: pk_error = %d", prefix, pk_error);
  }

  pk_error = ERR_ALL_IS_WELL;
}

