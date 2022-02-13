/*  -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/*\
|*|
|*| foobar-gnunet.c
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

	@file       foobar-gnunet.c
	@brief      Functions for the GNUnet thread; every time we want to signal
	            the GTK thread we use `g_idle_add()`.

**/


#include <stdio.h>
#include <stdatomic.h>
#include <gnunet/platform.h>
#include <gnunet/gnunet_fs_service.h>
#include <glib.h>
#include "foobar-common.h"
#include "foobar-gnunet.h"
#include "foobar-ui.h"


/**

	@brief      The filesharing handle

**/
static struct GNUNET_FS_Handle * fs_handle;


/**

	@brief      The indexed context

**/
static struct GNUNET_FS_GetIndexedContext * indexed_context;


/**

	@brief      Last function automatically invoked before shutdown
	@param      v_app_data      The data shared between threads, passed as a
	                            `void *` pointer
	@return     Nothing

**/
void clear_query_context (
	void * const v_app_data
) {

	#define app_data ((AppData *) v_app_data)

	atomic_store(&app_data->worker_is_running, false);
	g_clear_pointer(&indexed_context, GNUNET_FS_get_indexed_files_cancel);
	g_clear_pointer(&fs_handle, GNUNET_FS_stop);
	fprintf(stderr, "The GNUnet worker has returned\n");

	if (atomic_load(&app_data->ui_is_running)) {

		g_idle_add(ui_quit_idle, app_data->ui_app);

	}

	#undef app_data

}


/**

	@brief      Callback function invoked for each indexed file found
	@param      v_app_data      The data shared between threads, passed as a
	                            `void *` pointer
	@param      path            The path of the indexed file
	@param      file_id         The file's id
	@return     Either `GNUNET_OK` or `GNUNET_SYSERR`

**/
static int foreach_indexed (
	void * const v_app_data,
	const char * const path,
	const struct GNUNET_HashCode * const file_id
) {

	#define app_data ((AppData *) v_app_data)

	if (!path) {

		fprintf(stderr, "List of files received.\n");
		indexed_context = NULL;
		g_mutex_lock(&app_data->fs_query.indexed_mutex);
		app_data->fs_query.state = QUERY_COMPLETED;
		g_mutex_unlock(&app_data->fs_query.indexed_mutex);
		g_idle_add(query_callback_idle, app_data->ui_private);
		return GNUNET_SYSERR;

	}

	g_mutex_lock(&app_data->fs_query.indexed_mutex);

	app_data->fs_query.paths = g_list_append(
		app_data->fs_query.paths,
		g_strdup(path)
	);

	g_mutex_unlock(&app_data->fs_query.indexed_mutex);
	return GNUNET_OK;

	#undef app_data

}


/**

	@brief      Function invoked via `GNUNET_WORKER_push_load()` which queries
	            the list of the indexed files
	@param      v_app_data      The data shared between threads, passed as a
	                            `void *` pointer
	@return     Nothing

**/
void query_indexed_files (
	void * const v_app_data
) {

	#define app_data ((AppData *) v_app_data)

	fprintf(stderr, "Querying the GNUnet FS service...\n");
	g_mutex_lock(&app_data->fs_query.indexed_mutex);
	g_list_free_full(app_data->fs_query.paths, g_free);
	app_data->fs_query.paths = NULL;

	indexed_context = GNUNET_FS_get_indexed_files(
		fs_handle,
		&foreach_indexed,
		v_app_data
	);

	app_data->fs_query.state =
		indexed_context ?
			QUERY_RUNNING
		:
			QUERY_FAILED;

	g_mutex_unlock(&app_data->fs_query.indexed_mutex);
	g_idle_add(query_callback_idle, app_data->ui_private);

	#undef app_data

}


/**

	@brief      Function invoked via `GNUNET_WORKER_push_load()` which cancels
	            the current query (if any)
	@param      v_app_data      The data shared between threads, passed as a
	                            `void *` pointer
	@return     Nothing

**/
void cancel_indexed_query (
	void * const v_app_data
) {

	#define app_data ((AppData *) v_app_data)

	g_clear_pointer(
		&indexed_context,
		GNUNET_FS_get_indexed_files_cancel
	);

	g_mutex_lock(&app_data->fs_query.indexed_mutex);
	app_data->fs_query.state = QUERY_OFF;
	g_mutex_unlock(&app_data->fs_query.indexed_mutex);
	g_idle_add(query_callback_idle, app_data->ui_private);
	fprintf(stderr, "Query has been cancelled\n");

	#undef app_data

}


/**

	@brief      First function automatically invoked when the scheduler is
	            launched
	@param      v_app_data      The data shared between threads, passed as a
	                            `void *` pointer
	@return     `1` if the scheduler must stay alive, `0` if it must shut down

**/
GNUNET_WORKER_LifeInstructions fs_service_start_check (
	void * const v_app_data
) {

	#define app_data ((AppData *) v_app_data)

	fs_handle = GNUNET_FS_start(
		app_data->gnunet_config,
		"foobar",
		NULL,
		v_app_data,
		GNUNET_FS_FLAGS_NONE,
		GNUNET_FS_OPTIONS_END
	);

	if (!fs_handle) {

		/*  The worker will shut down!  */

		fprintf(
			stderr,
			"Unable to interrogate the filesharing service - abort\n"
		);

		return GNUNET_WORKER_DESTRUCTION;

	}

	atomic_store(&app_data->worker_is_running, true);
	return GNUNET_WORKER_LONG_LIFE;

	#undef app_data

}


/*  EOF  */

