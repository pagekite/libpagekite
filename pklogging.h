/******************************************************************************
utils.h - Logging.

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

#define PK_LOG_TUNNEL_DATA     0x0001
#define PK_LOG_TUNNEL_HEADERS  0x0002
#define PK_LOG_TUNNEL_CONNS    0x0004
#define PK_LOG_BE_DATA         0x0010
#define PK_LOG_BE_HEADERS      0x0020
#define PK_LOG_BE_CONNS        0x0040

#define PK_LOG_ERROR           0x1000

#define PK_LOG_CONNS           PK_LOG_BE_CONNS | PK_LOG_TUNNEL_CONNS
#define PK_LOG_NORMAL          PK_LOG_ERROR | PK_LOG_CONNS
#define PK_LOG_ALL             0xffff

int pk_log(int, const char *fmt, ...);
int pk_log_chunk(struct pk_chunk* chnk);
