/******************************************************************************
pklua.c - Allow libpagekite to be extended using the lua language

This file is Copyright 2011-2015, The Beanstalks Project ehf.

This program is free software: you can redistribute it and/or modify it under
the terms  of the  Apache  License 2.0  as published by the  Apache  Software
Foundation.

This program is distributed in the hope that it will be useful,  but  WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the Apache License for more details.

You should have received a copy of the Apache License along with this program.
If not, see: <http://www.apache.org/licenses/>

Note: For alternate license terms, see the file COPYING.md.

*******************************************************************************
How it works:

   - The Main event loop owns the "master Lua context".
   - Each blocker thread has their own secondary Lua context.
   - All requested plugins are loaded into each Lua context.
   - This C wrapper provides a bridge between secondary Luas and the master.
   - This C wrapper provides a rendezvous point for libpagekite hooks and
     Lua plugin callbacks.

******************************************************************************/
#define PAGEKITE_CONSTANTS_ONLY
#include "pagekite.h"

#include "common.h"
#include "utils.h"
#include "pkstate.h"
#include "pkerror.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pkrelay.h"
#include "pklogging.h"

#ifdef HAVE_LUA
#include "pklua.h"
#include "pklualua.h"

#include <lauxlib.h>
#include <lualib.h>


/*** LUA extensions to interact with libpagekite ****************************/

int pklua_lua_pklua_log_error(lua_State* L)
{
  pk_log(PK_LOG_ERROR, "Lua error: %s", lua_tostring(L, -1));
  return 0;
}

int pklua_lua_pklua_log_debug(lua_State* L)
{
  pk_log(PK_LOG_LUA_DEBUG, "%s", lua_tostring(L, -1));
  return 0;
}

int pklua_lua_pklua_log(lua_State* L)
{
  pk_log(PK_LOG_LUA_INFO, "%s", lua_tostring(L, -1));
  return 0;
}

typedef struct pklua_sock_ {
  int sockfd;
} pklua_sock;

int _pklua_lua_socket_get_sockfd(lua_State* L)
{
  if (lua_gettop(L) > 0 && lua_istable(L, 1)) {
    lua_getfield(L, 1, "_sockfd");
    if (lua_isnumber(L, -1)) {
      int sockfd = lua_tointeger(L, -1);
      lua_remove(L, -1);
      return sockfd;
    }
    lua_remove(L, -1);
  } 
  lua_pushstring(L, "Incorrect arguments");
  lua_error(L);
  return -1;
}

typedef struct pklua_buffer_ {
  size_t length;
  const char* bytes;
} pklua_buffer;

void _pklua_lua_socket_get_buffer(lua_State* L,
                                  const char* name, pklua_buffer* buf)
{
  if (lua_gettop(L) > 0 && lua_istable(L, 1)) {
    lua_getfield(L, 1, name);
    if (lua_isstring(L, -1)) {
      buf->bytes = lua_tolstring(L, -1, &(buf->length));
      return;
    }
    lua_remove(L, -1);
  } 
  lua_pushstring(L, "Incorrect arguments");
  lua_error(L);
}
void _pklua_lua_socket_free_buffer(lua_State* L, int stack_pos)
{
  lua_remove(L, stack_pos);
}
void _pklua_lua_socket_shift_buffer(lua_State* L, const char* name,
                                    size_t pos, pklua_buffer* buf)
{
  if (pos > buf->length) pos = buf->length;
  lua_pushlstring(L, buf->bytes + pos, buf->length - pos);
  lua_setfield(L, 1, name);
}
void _pklua_lua_socket_append_buffer(lua_State* L, const char* name,
                                     const char* bytes, size_t length)
{
  lua_getfield(L, 1, name);
  lua_pushlstring(L, bytes, length);
  lua_concat(L, 2);
  lua_setfield(L, 1, name);
}

int pklua_lua_socket_close(lua_State* L)
{
  pk_log(PK_LOG_LUA_DEBUG, "pklua_lua_socket_close(%p)", L);
  int sockfd = _pklua_lua_socket_get_sockfd(L);
  if (sockfd > -1) PKS_close(sockfd);
  return 0;
}

int pklua_lua_socket_send(lua_State* L)
{
  pk_log(PK_LOG_LUA_DEBUG, "pklua_lua_socket_send(%p)", L);
  int sockfd = _pklua_lua_socket_get_sockfd(L);
  if (lua_gettop(L) > 1 && lua_isstring(L, 2)) {
    ssize_t dl;
    size_t data_len;
    const char *data = lua_tolstring(L, 2, &data_len);
    if ((dl = data_len) != PKS_write(sockfd, data, data_len)) {
      pk_log(PK_LOG_LUA_DEBUG, "write failed to %d", sockfd);
      return 0;
    }
    lua_pushinteger(L, data_len);
    return 1;
  }
  lua_pushstring(L, "Incorrect arguments");
  lua_error(L);
  return -1;
}

int pklua_lua_socket_peek(lua_State *L)
{
  pk_log(PK_LOG_LUA_DEBUG, "pklua_lua_socket_peek(%p)", L);
  int sockfd = _pklua_lua_socket_get_sockfd(L);
  /* FIXME! This will be needed for implementing a pagekite front-end */
  return 0;
}

int pklua_lua_socket_recv(lua_State *L)
{
  pk_log(PK_LOG_LUA_DEBUG, "pklua_lua_socket_recv(%p)", L);

  pklua_buffer read_buffer;
  int sockfd = _pklua_lua_socket_get_sockfd(L);
  unsigned int wantbytes = 0;
  unsigned int maxbytes = 8*1024;
  char sock_input[4096];

  if (lua_gettop(L) > 1 && lua_isnumber(L, 2))
    wantbytes = lua_tointeger(L, 2);
  if (lua_gettop(L) > 2 && lua_isnumber(L, 3))
    maxbytes = lua_tointeger(L, 3);
  if (maxbytes < wantbytes)
    maxbytes = wantbytes;

  _pklua_lua_socket_get_buffer(L, "_read_buffer", &read_buffer);
  int buffer_pos = lua_gettop(L);
  size_t i = 0;
  while (1) {
    /* Check if we have a line in our buffer, return if so! */
    for (; i < read_buffer.length; i++) {
      if (((wantbytes < 1) && (read_buffer.bytes[i] == '\n')) ||
          ((wantbytes > 0) && (i >= wantbytes)) ||
          (i >= maxbytes)) {
        /* Copy the line/chunk... */
        size_t eol = i;
        if (wantbytes < 1)
          while (eol && (read_buffer.bytes[eol] == '\r' ||
                         read_buffer.bytes[eol] == '\n')) eol--;
        lua_pushlstring(L, read_buffer.bytes, eol+1);
        /* Adjust the read_buffer and pop from the stack */
        _pklua_lua_socket_shift_buffer(L, "_read_buffer", i+1, &read_buffer);
        _pklua_lua_socket_free_buffer(L, buffer_pos);
        /* Return line! */
        return 1;
      }
    }
    /* Only have partial line, read more data from socket */
    int read_bytes = wantbytes ? (wantbytes - read_buffer.length) : 4096;
    if (read_bytes > 4096) read_bytes = 4096;
    while ((read_bytes = PKS_read(sockfd, sock_input, read_bytes)) < 0) {
       /* Is this really an error? */      
       if ((errno != 0) && (errno != EINTR) && (errno != EAGAIN)) break;
    }
    /* If EOF, return buffer contents */
    if (read_bytes <= 0) {
      lua_pushlstring(L, read_buffer.bytes, read_buffer.length);
      /* Adjust the read_buffer and pop from the stack */
      _pklua_lua_socket_shift_buffer(L, "_read_buffer", i+1, &read_buffer);
      _pklua_lua_socket_free_buffer(L, buffer_pos);
      /* Return line! */
      return 1;
    }
    /* Else, append to buffer, keep looping */
    else {
      _pklua_lua_socket_free_buffer(L, buffer_pos);
      _pklua_lua_socket_append_buffer(L, "_read_buffer",
                                      sock_input, read_bytes);
      _pklua_lua_socket_get_buffer(L, "_read_buffer", &read_buffer);
      buffer_pos = lua_gettop(L);
    }
  }
  _pklua_lua_socket_free_buffer(L, buffer_pos);
  return 0;
}

typedef struct pklua_socket_server_cb_data_ {
  struct pk_manager* pkm;
  char               name[];
} pklua_socket_server_cb_data;

static void pklua_socket_server_accepted_cb(int sockfd, void* void_data) {
  pk_log(PK_LOG_LUA_DEBUG,
         "pklua_socket_server_accepted_cb(%d, %p)", sockfd, void_data);
  pklua_socket_server_cb_data* data = (pklua_socket_server_cb_data*) void_data;
  pkb_add_job(&(data->pkm->blocking_jobs), PK_ACCEPT_LUA, sockfd, void_data);
}

int pklua_lua_pkm_add_socket_server(lua_State *L)
{
  pk_log(PK_LOG_LUA_DEBUG, "pklua_lua_pkm_add_socket_server(%p)", L);
  int n = lua_gettop(L);
  if (n != 4 ||
      !lua_istable(L, 1) ||
      !lua_isstring(L, 2) ||
      !lua_isstring(L, 3) ||
      !lua_isnumber(L, 4)) {
    lua_pushstring(L, "Incorrect arguments");
    return lua_error(L);
  } 
  lua_getfield(L, 1, "_pkm");
  if (!lua_islightuserdata(L, -1)) {
    lua_pushstring(L, "Incorrect arguments");
    return lua_error(L);
  }

  struct pk_manager* pkm = lua_touserdata(L, -1);
  lua_remove(L, -1);
  char* name = lua_tostring(L, 2);
  char* host = lua_tostring(L, 3);
  int port = lua_tointeger(L, 4);

  pklua_socket_server_cb_data* data = malloc(sizeof(pklua_socket_server_cb_data)
                                             + strlen(name) + 1); 
  strcpy(data->name, name);
  data->pkm = pkm;
  int lport = pkm_add_listener(pkm, host, port,
                               &pklua_socket_server_accepted_cb,
                               (void *) data);
  if (lport > 0) {
    pk_log(PK_LOG_LUA_INFO,
           "Listening for %s on %s:%d (port %d)", name, host, port, lport);
    return 0;
  }
  else {
    pk_log(PK_LOG_ERROR,
           "Failed to add listener %s on %s:%d", name, host, port);
    lua_pushstring(L, "Failed to add listener");
    return lua_error(L);
  }
}


/*** Internals **************************************************************/

static const luaL_Reg pklua_methods[] = {
  { "log_error", pklua_lua_pklua_log_error },
  { "log_debug", pklua_lua_pklua_log_debug },
  { "log",       pklua_lua_pklua_log },
  { NULL,        NULL }
};

/* Create a Lua object wrapper around a socket, push onto the Lua stack */
void pklua_wrap_sock(lua_State* L, int sockfd)
{
  static const luaL_Reg sock_methods[] = {
    { "__gc",    pklua_lua_socket_close },
    { "close",   pklua_lua_socket_close },
    { "send",    pklua_lua_socket_send },
    { "peek",    pklua_lua_socket_peek },
    { "receive", pklua_lua_socket_recv },
    { NULL,      NULL }
  };
  lua_newtable(L);
  lua_pushinteger(L, sockfd);
  lua_setfield(L, -2, "_sockfd");
  lua_pushstring(L, "");
  lua_setfield(L, -2, "_read_buffer");
  for (const luaL_Reg* reg = sock_methods; reg->func != NULL; reg++) {
    lua_pushcfunction(L, reg->func);
    lua_setfield(L, -2, reg->name);
  }
}

/* Create a Lua object wrapper around a pkmanager, push onto the Lua stack */
void pklua_wrap_pk_manager(lua_State* L, struct pk_manager* pkm)
{
  static const luaL_Reg pkm_methods[] = {
    { "_add_socket_server", pklua_lua_pkm_add_socket_server },
    { NULL,                 NULL }
  };
  lua_newtable(L);
  lua_pushlightuserdata(L, pkm);
  lua_setfield(L, -2, "_pkm");
  for (const luaL_Reg* reg = pkm_methods; reg->func != NULL; reg++) {
    lua_pushcfunction(L, reg->func);
    lua_setfield(L, -2, reg->name);
  } 
}


/*** Libpagekite-facing code ************************************************/

lua_State* pklua_get_lua(struct pk_manager* pkm) {
  lua_State* L = lua_open();
  const luaL_Reg* reg;

  /* FIXME: We would rather not use luaL_openlibs(L), because we probably
   *        want a stricter sandbox? */;
  luaL_openlibs(L);

  /* Load compiled-in pklua.lua + plugins */
  if ((0 != luaL_loadstring(L, pklualua)) ||
      (0 != lua_pcall(L, 0, LUA_MULTRET, 0))) {
    pk_log(PK_LOG_ERROR, "Lua init failed: %s", lua_tostring(L, -1));
    return NULL;
  }
  else {
    /* Add some native methods, register as pklua */
    for (reg = pklua_methods; reg->func != NULL; reg++) {
      luaL_register(L, NULL, reg);
    } 
    lua_setglobal(L, "pklua");

    lua_getfield(L, LUA_GLOBALSINDEX, "pklua");
    lua_pushstring(L, PK_VERSION);
    lua_setfield(L, -2, "version");
    lua_remove(L, -1);

    pklua_configure(L, pkm);
    luaL_dostring(L, "pklua:log('Loaded pklua.lua v' .. pklua.version);");
  }

  return L;
}

void pklua_close_lua(lua_State* L)
{
  if (L != NULL) lua_close(L);
}

int pklua_configure(lua_State* L, struct pk_manager* pkm)
{
  pk_log(PK_LOG_LUA_DEBUG, "pklua_configure(%p, %p)", L, pkm);
  lua_getfield(L, LUA_GLOBALSINDEX, "pklua");

  lua_getfield(L, -1, "_enable_defaults");
  lua_pushvalue(L, -2);
  lua_pushboolean(L, pkm->lua_enable_defaults);
  lua_call(L, 2, 0);

  for (const char** c = pkm->lua_settings; c && *c; c++) {
    lua_getfield(L, -1, "_set_setting");
    lua_pushvalue(L, -2);
    lua_pushstring(L, *c);
    lua_call(L, 2, 0);
  }

  lua_remove(L, -1);
  return 0;
}

int pklua_add_listeners(lua_State* L, struct pk_manager* pkm) {
  pk_log(PK_LOG_LUA_DEBUG, "pklua_add_listeners(%p, %p)", L, pkm);
  lua_getfield(L, LUA_GLOBALSINDEX, "pklua");
  lua_getfield(L, -1, "_configure_socket_servers");
  lua_pushvalue(L, -2);
  pklua_wrap_pk_manager(L, pkm);
  lua_call(L, 2, 0);
  return 0;
}

void pklua_socket_server_accepted(lua_State* L, int sockfd, void* void_data) {
  pk_log(PK_LOG_LUA_DEBUG,
         "pklua_socket_server_accepted(%p, %d, %p)", L, sockfd, void_data);
  pklua_socket_server_cb_data* data = (pklua_socket_server_cb_data*) void_data;
  lua_getfield(L, LUA_GLOBALSINDEX, "pklua");
  lua_getfield(L, -1, "_socket_server_accept");
  lua_pushvalue(L, -2);
  pklua_wrap_pk_manager(L, data->pkm);
  lua_pushstring(L, data->name);
  pklua_wrap_sock(L, sockfd);
  lua_pcall(L, 4, 0, 0);
}

#endif
