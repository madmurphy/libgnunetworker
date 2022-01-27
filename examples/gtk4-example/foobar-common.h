/*  -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/*\
|*|
|*| foobar-common.h
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

	@file       foobar-common.h
	@brief      Functions and data types shared among all Foobar's modules

**/


#ifndef __FOOBAR_COMMON_H__
#define __FOOBAR_COMMON_H__


#include <stdatomic.h>
#include <gtk/gtk.h>
#include <gnunet/gnunet_worker_lib.h>


/**

	@brief      Possible states of a query

**/
enum QueryState {
	QUERY_OFF,
	QUERY_FAILED,
	QUERY_RUNNING,
	QUERY_COMPLETED
};


/**

	@brief      The data shared between threads

**/
typedef struct AppData_T {
	int argc;
	const char * const * argv;
	int gtk_status;
	const char * cfg_path;
	struct {
		enum QueryState state;
		GList * paths;
		GMutex indexed_mutex;
	} fs_query;
	const struct GNUNET_CONFIGURATION_Handle * gnunet_config;
    GNUNET_WORKER_Handle * gnunet_worker;
	gpointer ui_private;
	GtkApplication * ui_app;
	atomic_bool worker_is_running;
	atomic_bool ui_is_running;
} AppData;


#endif


/*  EOF  */

