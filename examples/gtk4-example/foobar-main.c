/*  -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/*\
|*|
|*| foobar-main.c
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

	@file       foobar-main.c
	@brief      Foobar's `main()` function

**/


/*  This will be defined via `configure` script eventually...  */
#define GETTEXT_PACKAGE "Foobar"


#include <stdbool.h>
#include <stddef.h>
#include <gtk/gtk.h>

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <gnunet/gettext.h>
#include <gnunet/gnunet_util_lib.h>
#include <gnunet/gnunet_worker_lib.h>
#include "foobar-common.h"
#include "foobar-gnunet.h"
#include "foobar-ui.h"


/**

	@brief      This structure holds informations about the project

**/
static const struct GNUNET_OS_ProjectData foobar_pd = {
	.libname = "libfoobar",
	.project_dirname = "foobar",
	.binary_name = "foobar",
	.env_varname = "FOOBAR_PREFIX",
	.base_config_varname = "FOOBAR_BASE_CONFIG",
	.bug_email = "developers@foobar.org",
	.homepage = "http://www.foobar.org/",
	.config_file = "foobar.conf",
	.user_config_file = "~/.config/foobar.conf",
	.version = "0.1",
	.is_gnu = 1,
	.gettext_domain = GETTEXT_PACKAGE,
	.gettext_path = NULL,
	.agpl_url = "http://www.foobar.org/COPYING",
};


/**

	@brief      Main function that will start the worker and the GTK thread
	@param      v_app_data      The data shared between threads, passed as a
	                            `void *` pointer
	@param      args            The remaining command-line arguments
	@param      cfg_path         Name of the configuration file used (for
	                            saving, can be NULL!)
	@param      config          Configuration
	@return     Nothing

**/
static void foobar_main (
	void * const v_app_data,
	char * const * const args,
	const char * const cfg_path,
	const struct GNUNET_CONFIGURATION_Handle * const config
) {

	#define app_data ((AppData *) v_app_data)

	register int argc = 0;

	while (args[argc]) {

		argc++;

	}

	app_data->argc = argc;
	app_data->argv = (const char * const *) args;
	app_data->cfg_path = cfg_path;
	app_data->gnunet_config = config;
	app_data->worker_is_running = false;
	app_data->ui_is_running = false;
	app_data->fs_query.paths = NULL;
	g_mutex_init(&app_data->fs_query.indexed_mutex);

	GNUNET_WORKER_start_serving(
		&app_data->gnunet_worker,
		&gtk_main_with_gnunet_worker,
		&fs_service_start_check,
		&clear_query_context,
		v_app_data
	);

	/*

	For no particular reason we decided to run the GNUnet scheduler in the main
	thread and the GTK event loop in another thread. If you want to swap the
	two and run the GTK application in the main thread instead, remove the
	previous invocation of `GNUNET_WORKER_start_serving()` and uncomment the
	following lines:

	*/

	/*

	if (
		!GNUNET_WORKER_create(
			&app_data->gnunet_worker,
			&fs_service_start_check,
			&clear_query_context,
			v_app_data
		)
	) {

		gtk_main_with_gnunet_worker(app_data->gnunet_worker, v_app_data);

	}

	*/

	g_mutex_clear(&app_data->fs_query.indexed_mutex);

	#undef app_data
}


/**

	@brief      The main function to foobar
	@param      argc            Number of arguments from the command line
	@param      argv            Command line arguments
	@return     0 ok, 1 on error

	We filter the startup through `GNUNET_PROGRAM_run2`. Depending on how much
	your application is a GNUnet application or instead an application that
	uses GNUnet only for minor reasons, you might want to start your program
	differently.

**/
int main (
	const int argc,
	const char * const * const argv
) {

	static const struct GNUNET_GETOPT_CommandLineOption options[] = {
		GNUNET_GETOPT_OPTION_END
	};

	static AppData app_data;

	GNUNET_OS_init(&foobar_pd);

	/*  **IMPORTANT** We use `GNUNET_PROGRAM_run2` with `GNUNET_YES` as last
		argument to avoid that GNUnet's scheduler is automatically started  */

	return
		GNUNET_PROGRAM_run2(
			argc,
			(char * const *) argv,
			"foobar [options [value]]",
			gettext_noop("foobar"),
			options,
			&foobar_main,
			&app_data,
			GNUNET_YES
		) ? app_data.gtk_status : 1;

}


/*  EOF  */

