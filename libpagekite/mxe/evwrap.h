/******************************************************************************
evwrap.h - A wrapper around libev, for building on Windows.

This file is Copyright 2011-2025, The Beanstalks Project ehf.

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

#define EV_STANDALONE 1            /* keeps ev from requiring config.h */
#define EV_SELECT_IS_WINSOCKET 1   /* configure libev for windows select */
#include <libev/ev.h>
