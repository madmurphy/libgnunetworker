/*  -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/*\
|*|
|*| foobar/src/foobar-ui.h
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

	@file		foobar-ui.h
	@brief		Public functions and data types of `foobar-ui.c`

**/


#ifndef __FOOBAR_UI_H__
#define __FOOBAR_UI_H__


#include <gtk/gtk.h>
#include <gnunet/gnunet_worker_lib.h>


extern gboolean query_callback_idle (
	gpointer v_ui_data
);


extern void gtk_main_with_gnunet_worker (
    GNUNET_WORKER_Handle * worker_handler,
    gpointer v_app_data
);


#endif


/*  EOF  */

