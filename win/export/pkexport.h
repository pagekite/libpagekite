/******************************************************************************
pkexport.h - A wrapper for building the libpagekite library as a dll.

*******************************************************************************

This file is Copyright 2012-2013, The Beanstalks Project ehf.

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

#ifndef _PAGEKITEC_DLL_H
#define _PAGEKITEC_DLL_H

#include "common.h"
#include "pkstate.h"
#include "pkerror.h"
#include "pkconn.h"
#include "pkproto.h"
#include "pkblocker.h"
#include "pkmanager.h"
#include "pklogging.h"
#include "version.h"

#include "pagekite_net.h"

__declspec(dllexport) int libpagekite_init(int kites, int max_conns,
                                           int static_setup, int spare_frontends, 
                                           int verbosity);

__declspec(dllexport) int libpagekite_useWatchdog();

__declspec(dllexport) int libpagekite_useEvil();

__declspec(dllexport) int libpagekite_addKite(char* proto, char* kitename, int pport,
                                              char* secret, char* backend, int lport);

__declspec(dllexport) int libpagekite_addFrontend(char* domain, int port);

__declspec(dllexport) int libpagekite_tick();

__declspec(dllexport) int libpagekite_poll(int timeout);

__declspec(dllexport) int libpagekite_start();

__declspec(dllexport) int libpagekite_stop();

__declspec(dllexport) int libpagekite_getStatus();

__declspec(dllexport) char* libpagekite_getLog();

#endif