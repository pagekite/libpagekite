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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#ifdef BUILDING_PAGEKITE_DLL
#define DECLSPEC_DLL __declspec(dllexport)
#else
#define DECLSPEC_DLL __declspec(dllimport)
#endif
#else
#define DECLSPEC_DLL 
#endif

DECLSPEC_DLL int libpagekite_init(int kites,
                                           int max_conns,
                                           int static_setup,
                                           int spare_frontends, 
                                           int verbosity);
DECLSPEC_DLL int libpagekite_useWatchdog();
DECLSPEC_DLL int libpagekite_useEvil();
DECLSPEC_DLL int libpagekite_addKite(char* proto,
                                              char* kitename,
                                              int pport,
                                              char* secret,
                                              char* backend,
                                              int lport);
DECLSPEC_DLL int libpagekite_addFrontend(char* domain, int port);
DECLSPEC_DLL int libpagekite_tick();
DECLSPEC_DLL int libpagekite_poll(int timeout);
DECLSPEC_DLL int libpagekite_start();
DECLSPEC_DLL int libpagekite_stop();
DECLSPEC_DLL int libpagekite_getStatus();
DECLSPEC_DLL char* libpagekite_getLog();

#ifdef __cplusplus
}
#endif

#endif
