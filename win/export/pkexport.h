/******************************************************************************
pkexport.h - A wrapper for building the libpagekite library as dll.

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

/* THIS FILE IS A WORK IN PROGRESS */

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

__declspec(dllexport) int pagekitec_init(int kites);

__declspec(dllexport) int pagekitec_addKite(char* proto, char* kitename, int pport,
                                            char* secret, char* backend, int lport);

__declspec(dllexport) int pagekitec_addFrontend(char* domain, int port);

__declspec(dllexport) int pagekitec_start();

__declspec(dllexport) int pagekitec_stop();

#endif