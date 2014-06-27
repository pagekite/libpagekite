/******************************************************************************
gettimeofday.h - A wrapper around gettimeofday for building on Windows

This file is Copyright 2011-2013, The Beanstalks Project ehf.

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


// TODO: get permission to use this
// http://www.interopcommunity.com/dictionary/gettimeofday-entry.php
//
#ifdef _MSC_VER
int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif