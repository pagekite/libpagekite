/******************************************************************************
pklua.h - Allow libpagekite to be extended using the lua language

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

******************************************************************************/
#ifdef HAVE_LUA

pk_lua_t* pklua_get_locked_lua(struct pk_manager*);
pk_lua_t* pklua_lock_lua(pk_lua_t*);
pk_lua_t* pklua_unlock_lua(pk_lua_t*);
void pklua_close_lua(pk_lua_t*);
void pklua_set_thread_lua(pk_lua_t*);
void pklua_remove_thread_lua();
int pklua_configure(pk_lua_t*, struct pk_manager*);
int pklua_add_listeners(pk_lua_t*);
void pklua_socket_server_accepted(pk_lua_t*, int, void*);

#else 
#define pklua_get_locked_lua(m)                (m) ? NULL : NULL;
#define pklua_lock_lua(l)                      (void) l;
#define pklua_unlock_lua(l)                    (void) l;
#define pklua_close_lua(l)                     (void) l;
#define pklua_set_thread_lua(l)                (void) l;
#define pklua_remove_thread_lua()              (void);
#define pklua_configure(l, m)                  (void) l; (void) m;
#define pklua_add_listeners(l)                 (void) l;
#define pklua_socket_server_accepted(l, s, d)  (void) l; (void) s; (void) d;
#endif
