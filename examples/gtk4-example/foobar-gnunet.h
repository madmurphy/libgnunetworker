/*  -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/*\
|*|
|*| foobar/src/foobar-gnunet.h
|*|
|*| https://gitlab.com/authors/foobar
|*|
|*| Copyright (C) 2022 authors <authors@example.com>
|*|
|*| **Foobar** is free software: you can redistribute it and/or modify it under
|*| the terms of the GNU General Public License as published by the Free
|*| Software Foundation, either version 3 of the License, or (at your option)
|*| any later version.
|*|
|*| **Foobar** is distributed in the hope that it will be useful, but WITHOUT
|*| ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
|*| FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
|*| more details.
|*|
|*| You should have received a copy of the GNU General Public License along
|*| with this program. If not, see <http://www.gnu.org/licenses/>.
|*|
\*/


/**

	@file		foobar-gnunet.h
	@brief		Public functions and data types of `foobar-gnunet.c`

**/


#ifndef __FOOBAR_GNUNET_H__
#define __FOOBAR_GNUNET_H__


#include <stdbool.h>


extern bool fs_service_start_check (
	void * const v_app_data
);


extern void clear_query_context (
	void * const v_app_data
);


extern void query_indexed_files (
	void * const v_app_data
);


extern void cancel_indexed_query (
	void * const v_app_data
);


#endif


/*  EOF  */

