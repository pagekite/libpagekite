/******************************************************************************
pklua.h - Allow libpagekite to be extended using the lua language

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

******************************************************************************/
#ifdef HAVE_LUA

lua_State* pklua_get_lua(struct pk_manager*);
void pklua_close_lua(lua_State*);

int pklua_configure(lua_State*, struct pk_manager*);
int pklua_add_listeners(lua_State*, struct pk_manager*);
void pklua_socket_server_accepted(lua_State*, int, void*);

#else 
#define pklua_get_lua(m)                       NULL;
#define pklua_close_lua(l)                     (void) l;
#define pklua_configure(l, m)                  (void) m;
#define pklua_add_listeners(l, m)              (void) m;
#define pklua_socket_server_accepted(l, s, d)  (void) s;
#endif
