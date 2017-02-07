/******************************************************************************
pklua.c - Allow libpagekite to be extended using the lua language

This file is Copyright 2011-2017, The Beanstalks Project ehf.

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
How it should work:

   - The Main event loop owns the "master Lua context".
   - Each blocker thread has their own secondary Lua context.
   - All requested plugins are loaded into each Lua context.
   - This C wrapper provides a bridge between secondary Luas and the master.
   - This C wrapper provides a rendezvous point for libpagekite hooks and
     Lua plugin callbacks.

******************************************************************************/
#define PAGEKITE_CONSTANTS_ONLY
#include "pagekite.h"

#include "pkcommon.h"
#include "pkutils.h"
#include "pkhooks.h"
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

pk_lua_t*        pk_lua_thread_map = NULL;
pthread_mutex_t* pk_lua_thread_map_lock = NULL;


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
  lua_pushstring(L, "Incorrect arguments in get_sockfd");
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
  lua_pushstring(L, "Incorrect arguments in socket_get_buffer");
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
  lua_pushstring(L, "Incorrect arguments in socket_send");
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

static int pklua_socket_server_accepted_cb(int sockfd, void* void_data) {
  pk_log(PK_LOG_LUA_DEBUG,
         "pklua_socket_server_accepted_cb(%d, %p)", sockfd, void_data);
  pklua_socket_server_cb_data* data = (pklua_socket_server_cb_data*) void_data;
  pkb_add_job(&(data->pkm->blocking_jobs), PK_ACCEPT_LUA, sockfd, void_data);
  return 0;
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
    lua_pushstring(L, "Incorrect arguments in add_socket_server");
    return lua_error(L);
  }
  lua_getfield(L, 1, "_pkm");
  if (!lua_islightuserdata(L, -1)) {
    lua_pushstring(L, "Incorrect arguments in add_socket_server (2)");
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

int pklua_lua_pkm_get_metrics(lua_State* L)
{
  struct pk_global_state* state = &pk_state;

  pk_log(PK_LOG_LUA_DEBUG, "pklua_lua_pkm_get_metrics(%p)", L);
  int n = lua_gettop(L);
  if (n != 1 || !lua_istable(L, 1)) {
    lua_pushstring(L, "Incorrect arguments in get_metrics");
    return lua_error(L);
  }

  lua_getfield(L, 1, "_pkm");
  if (!lua_islightuserdata(L, -1)) {
    lua_pushstring(L, "Incorrect arguments in get_metrics (2)");
    return lua_error(L);
  }
  struct pk_manager* manager = lua_touserdata(L, -1);
  lua_remove(L, -1);

  lua_newtable(L);
  int metrics = lua_gettop(L); /* Relatives offsets would break below */

  lua_pushstring(L, PK_VERSION);
  lua_setfield(L, metrics, "libpagekite_version");
  #define PKM_INT(s, n) lua_pushinteger(L, s->n);\
                        lua_setfield(L, metrics, #s"_"#n)
  #define PKM_STR(s, n) lua_pushstring(L, (s->n == NULL) ? "(null)" : s->n);\
                        lua_setfield(L, metrics, #s"_"#n)
  PKM_INT(manager, status);
  PKM_INT(manager, last_world_update);
  PKM_INT(manager, next_tick);
  PKM_INT(manager, enable_timer);
  PKM_INT(manager, last_dns_update);
  PKM_INT(manager, kite_max);
  PKM_INT(manager, tunnel_max);
  PKM_INT(manager, be_conn_max);
  PKM_INT(manager, fancy_pagekite_net_rejection);
  PKM_INT(manager, enable_watchdog);
  PKM_INT(manager, want_spare_frontends);
  PKM_INT(manager, housekeeping_interval_min);
  PKM_INT(manager, housekeeping_interval_max);
  PKM_INT(manager, check_world_interval);
  PKM_STR(manager, dynamic_dns_url);
  PKM_INT(state, live_streams);
  PKM_INT(state, live_tunnels);
  PKM_INT(state, live_listeners);
  PKM_STR(state, app_id_short);
  PKM_STR(state, app_id_long);
  PKM_INT(state, quota_days);
  PKM_INT(state, quota_conns);
  PKM_INT(state, quota_mb);

  /* Copy Lua metrics from master Lua context */
  pk_lua_t* mgr_lua;
  if ((mgr_lua = pklua_lock_lua(manager->lua)) != NULL) {

    lua_State* ML = mgr_lua->lua;
    lua_getfield(ML, LUA_GLOBALSINDEX, "pklua");
    lua_getfield(ML, -1, "metrics");
    /* ML stack: -1=metrics, -2=pklua */

    lua_pushnil(ML);
    while (lua_next(ML, -2)) {
      /* copy key and value so lua_tostring doesn't corrupt */
      lua_pushvalue(ML, -2);
      lua_pushvalue(ML, -2);
      /* ML stack: -1=value copy, -2=key copy, -3=value, -4=key, -5=metrics */

      lua_pushinteger(L, lua_tointeger(ML, -1));
      lua_setfield(L, metrics, lua_tostring(ML, -2));

      lua_pop(ML, 3);
      /* ML stack: -1=key, -2=metrics */
    }

    lua_remove(ML, -1);
    lua_remove(ML, -1);
    pklua_unlock_lua(mgr_lua);
  }

  /* L stack: -1 = new table */
  return 1;
}

int pklua_hook_bridge(int hook_id, int iv, void* p1, void* p2);
int pklua_lua_pklua_bridge_hook_to_lua(lua_State *L)
{
  pk_log(PK_LOG_LUA_DEBUG, "pklua_lua_pklua_bridge_hook_to_lua(%p)", L);
  int n = lua_gettop(L);
  if (n != 2 ||
      !lua_istable(L, 1) ||
      !lua_isnumber(L, 2)) {
    lua_pushstring(L, "Incorrect arguments in bridge_hook_to_lua");
    return lua_error(L);
  }
  int hook_id = lua_tointeger(L, 2);
  lua_remove(L, 1);
  lua_remove(L, 1);

  pagekite_callback2_t* cb = &pklua_hook_bridge;
  if ((hook_id < 0) ||
      (hook_id >= PK_HOOK_MAX) ||
      ((pk_hooks[hook_id] != NULL) && (pk_hooks[hook_id] != cb))) {
    pk_log(PK_LOG_ERROR, "Hook invalid or already registered: %d", hook_id);
    lua_pushstring(L, "Hook invalid or already registered");
    return lua_error(L);
  }

  pk_hooks[hook_id] = cb;
  return 0;
}

int pklua_lua_pklua_get_hook_list(lua_State* L)
{
  pk_log(PK_LOG_LUA_DEBUG, "pklua_lua_pklua_get_hook_list(%p)", L);

  lua_newtable(L);
  #define PKM_HOOK(n) lua_pushinteger(L, n); lua_setfield(L, -2, #n)
  PKM_HOOK(PK_HOOK_STOPPED);
  PKM_HOOK(PK_HOOK_START_EV_LOOP);
  PKM_HOOK(PK_HOOK_START_BLOCKER);
  PKM_HOOK(PK_HOOK_LOG);
  PKM_HOOK(PK_HOOK_TICK);
  PKM_HOOK(PK_HOOK_CHECK_WORLD);
  PKM_HOOK(PK_HOOK_CHECK_TUNNELS);
  PKM_HOOK(PK_HOOK_STATE_CHANGED);
  PKM_HOOK(PK_HOOK_BE_CONN_WRAP);
  PKM_HOOK(PK_HOOK_FE_CONN_WRAP);
  PKM_HOOK(PK_HOOK_BE_CONN_OPENED);
  PKM_HOOK(PK_HOOK_BE_CONN_CLOSED);
  PKM_HOOK(PK_HOOK_FE_CONN_OPENED);
  PKM_HOOK(PK_HOOK_FE_CONN_CLOSED);
  PKM_HOOK(PK_HOOK_FE_DISCONNECT);
  PKM_HOOK(PK_HOOK_CHUNK_INCOMING);
  PKM_HOOK(PK_HOOK_CHUNK_OUTGOING);
  PKM_HOOK(PK_HOOK_DATA_INCOMING);
  PKM_HOOK(PK_HOOK_DATA_OUTGOING);

  return 1;
}


/*** Internals **************************************************************/

static const luaL_Reg pklua_methods[] = {
  { "log_error",           pklua_lua_pklua_log_error },
  { "log_debug",           pklua_lua_pklua_log_debug },
  { "log",                 pklua_lua_pklua_log },
  { "_bridge_hook_to_lua", pklua_lua_pklua_bridge_hook_to_lua },
  { "get_hook_list",       pklua_lua_pklua_get_hook_list },
  { NULL,        NULL }
};

/* Create a Lua object wrapper around a socket, push onto the Lua stack */
void _pklua_wrap_sock(lua_State* L, int sockfd)
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
void _pklua_wrap_pk_manager(lua_State* L, struct pk_manager* pkm)
{
  static const luaL_Reg pkm_methods[] = {
    { "_add_socket_server", pklua_lua_pkm_add_socket_server },
    { "get_metrics",        pklua_lua_pkm_get_metrics },
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

static pk_lua_t* get_thread_lua()
{
  /* FIXME: Configuring the thread list lock this way is a potential race.
     However, in practice the master "setup" thread should invoke this well
     before forking off any other threads, so not too worried. */
  if (pk_lua_thread_map_lock == NULL) {
    pk_lua_thread_map_lock = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(pk_lua_thread_map_lock, NULL);
  }

  pthread_mutex_lock(pk_lua_thread_map_lock);
  pk_lua_t* map = pk_lua_thread_map;
  while (map != NULL) {
    if (pthread_equal(pthread_self(), map->tid)) {
      pthread_mutex_unlock(pk_lua_thread_map_lock);
      return map;
    }
    map = (pk_lua_t*) map->next;
  }
  pthread_mutex_unlock(pk_lua_thread_map_lock);
  return NULL;
}

static pk_lua_t* pklua_new_thread_lua(pk_lua_t* src)
{
  pthread_mutex_lock(pk_lua_thread_map_lock);

  pk_lua_t** mapp = &pk_lua_thread_map;
  while (*mapp != NULL) mapp = (pk_lua_t**) &((*mapp)->next);

  pk_lua_t* map = *mapp = malloc(sizeof(pk_lua_t));
  if (src == NULL) {
    map->lock = malloc(sizeof(pk_rlock_t));
    pk_rlock_init(map->lock);
  }
  else {
    map->lock = src->lock;
  }
  pklua_lock_lua(map);

  map->tid = pthread_self();
  map->lua = (src == NULL) ? lua_open() : src->lua;
  map->is_copy = (src != NULL);
  map->next = NULL;

  pthread_mutex_unlock(pk_lua_thread_map_lock);
  return map;
}

void pklua_free_thread_lua(pk_lua_t* map)
{
  pthread_mutex_lock(pk_lua_thread_map_lock);
  pklua_lock_lua(map);

  pk_lua_t** mapp = &pk_lua_thread_map;
  while (*mapp != NULL) {
    if (*mapp == map) {
      pk_lua_t* mapn = map->next;
      if (!map->is_copy) {
        free(map->lock);
        lua_close(map->lua);
      }
      free(map);
      *mapp = mapn;
      pthread_mutex_unlock(pk_lua_thread_map_lock);
      return;
    }
    mapp = (pk_lua_t**) &((*mapp)->next);
  }
  pthread_mutex_unlock(pk_lua_thread_map_lock);
}

int pklua_configure(pk_lua_t *PL, struct pk_manager* pkm)
{
  lua_State* L = pklua_lock_lua(PL)->lua;

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

  lua_getfield(L, -1, "_init_plugins");
  lua_pushvalue(L, -2);
  lua_call(L, 1, 0);

  lua_remove(L, -1);

  pklua_unlock_lua(PL);
  return 0;
}

/* Returns the Lua object for this thread, or NULL if none exists.
 * If pkm != NULL, a new Lua object will be created and initialized
 * instead of returning NULL.
 */
pk_lua_t* pklua_get_unlocked_lua(struct pk_manager* pkm)
{
  pk_lua_t* PL = get_thread_lua();
  if ((PL != NULL) || (pkm == NULL)) return PL;

  /* No Lua object found, pkm != NULL => create new Lua state */
  PL = pklua_new_thread_lua(NULL);
  lua_State* L = PL->lua;
  const luaL_Reg* reg;

  /* FIXME: We would rather not use luaL_openlibs(L), because we probably
   *        want a stricter sandbox? */;
  luaL_openlibs(L);

  /* Load compiled-in pklua.lua + plugins */
  if ((0 != luaL_loadstring(L, (const char*) pklualua)) ||
      (0 != lua_pcall(L, 0, LUA_MULTRET, 0))) {
    pk_log(PK_LOG_ERROR, "Lua init failed: %s", lua_tostring(L, -1));
    pklua_unlock_lua(PL);
    return NULL;
  }

  /* Add some native methods, register as pklua */
  for (reg = pklua_methods; reg->func != NULL; reg++) {
    luaL_register(L, NULL, reg);
  }
  lua_setglobal(L, "pklua");

  lua_getfield(L, LUA_GLOBALSINDEX, "pklua");
  lua_pushstring(L, PK_VERSION);
  lua_setfield(L, -2, "version");
  _pklua_wrap_pk_manager(L, pkm);
  lua_setfield(L, -2, "manager");
  lua_remove(L, -1);

  pklua_configure(pklua_unlock_lua(PL), pkm);
  pklua_lock_lua(PL);
  luaL_dostring(L, "pklua:log('Loaded pklua.lua v' .. pklua.version);");

  return pklua_unlock_lua(PL);
}
pk_lua_t* pklua_get_locked_lua(struct pk_manager* pkm)
{
  return pklua_lock_lua(pklua_get_unlocked_lua(pkm));
}

pk_lua_t* pklua_lock_lua(pk_lua_t* PL) {
  if (PL != NULL) pk_rlock_lock(PL->lock);
  return PL;
}

pk_lua_t* pklua_unlock_lua(pk_lua_t* PL) {
  if (PL != NULL) pk_rlock_unlock(PL->lock);
  return PL;
}

void pklua_close_lua(pk_lua_t* PL)
{
  if (PL != NULL) pklua_free_thread_lua(PL);
}

void pklua_set_thread_lua(pk_lua_t* PL)
{
  assert(NULL == get_thread_lua());
  pklua_unlock_lua(pklua_new_thread_lua(PL));
}

void pklua_remove_thread_lua()
{
  pklua_free_thread_lua(pklua_unlock_lua(pklua_get_locked_lua(NULL)));
}

int pklua_add_listeners(pk_lua_t* PL) {
  lua_State* L = pklua_lock_lua(PL)->lua;
  pk_log(PK_LOG_LUA_DEBUG, "pklua_add_listeners(%p)", L);
  lua_getfield(L, LUA_GLOBALSINDEX, "pklua");
  lua_getfield(L, -1, "_configure_socket_servers");
  lua_pushvalue(L, -2);
  int rv = lua_pcall(L, 1, 0, 0);
  pklua_unlock_lua(PL);
  return rv;
}

void pklua_socket_server_accepted(pk_lua_t* PL, int sockfd, void* void_data) {
  lua_State* L = pklua_lock_lua(PL)->lua;
  pk_log(PK_LOG_LUA_DEBUG,
         "pklua_socket_server_accepted(%p, %d, %p)", L, sockfd, void_data);
  pklua_socket_server_cb_data* data = (pklua_socket_server_cb_data*) void_data;
  lua_getfield(L, LUA_GLOBALSINDEX, "pklua");
  lua_getfield(L, -1, "_socket_server_accept");
  lua_pushvalue(L, -2);
  lua_pushstring(L, data->name);
  _pklua_wrap_sock(L, sockfd);
  lua_pcall(L, 3, 0, 0);
  pklua_unlock_lua(PL);
}

int pklua_hook_bridge(int hook_id, int iv, void* p1, void* p2) {
  int rv = -1;

  pk_lua_t* PL = pklua_get_locked_lua(NULL);
  if (PL == NULL) {
    pk_log(PK_LOG_ERROR, "No Lua state found for hook %d", hook_id);
    return -1;
  }

  lua_State* L = PL->lua;
  lua_getfield(L, LUA_GLOBALSINDEX, "pklua");
  lua_getfield(L, -1, "_hook_bridge");
  lua_pushvalue(L, -2);
  lua_pushinteger(L, hook_id);
  lua_pushinteger(L, iv);
  if (p1) lua_pushlightuserdata(L, p1); else lua_pushnil(L);
  if (p2) lua_pushlightuserdata(L, p2); else lua_pushnil(L);
  if (lua_pcall(L, 5, 1, 0) == 0) {
    rv = lua_tointeger(L, -1);
    lua_remove(L, -1);
  }
  lua_remove(L, -1);

  pklua_unlock_lua(PL);
  return rv;
}

#endif
