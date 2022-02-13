/*  -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/*\
|*|
|*| foobar-ui.c
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

	@file       foobar-ui.c
	@brief      Functions for the GTK thread; every time we want to signal the
	            GNUnet thread we use `GNUNET_WORKER_push_load_with_priority()`
	            or `GNUNET_WORKER_push_load()`.

**/


#include <stdbool.h>
#include <stdatomic.h>
#include <gtk/gtk.h>
#include <gnunet/gnunet_worker_lib.h>
#include "foobar-common.h"
#include "foobar-ui.h"
#include "foobar-gnunet.h"


#define QUERY_TEXT "List published files"
#define CANCEL_TEXT "Cancel"
#define CLEAR_TEXT "Clear"


/**

	@brief      The data type that holds the private UI data

**/
typedef struct UISession_T {
	AppData * app_data;
	GNUNET_WORKER_Handle gnunet_worker;
	GtkListStore * list_store;
	GtkButton * query_button;
	GtkButton * reset_button;
} UISession;


/**

	@brief      Just a reminder for the published files `GtkListStore`

**/
enum {
	PF_COL_PATH = 0,
	PF_NUM_COLS
};


/**

	@brief      Function invoked by the worker thread via `g_idle_add()` to
	            quit the UI
	@param      v_ui_app        The application
	@return     Always `false`

**/
gboolean ui_quit_idle (
	gpointer v_ui_app
) {

	g_application_quit(G_APPLICATION(v_ui_app));
	return false;

}


/**

	@brief      Function invoked by the worker thread via `g_idle_add()` to
	            send a state update
	@param      v_ui_data       The private UI data, passed as a `void *`
	                            pointer
	@return     Always `false`

**/
gboolean query_callback_idle (
	gpointer v_ui_data
) {

	#define ui_data ((UISession *) v_ui_data)

	g_mutex_lock(&ui_data->app_data->fs_query.indexed_mutex);

	switch (ui_data->app_data->fs_query.state) {

		case QUERY_RUNNING:

			gtk_button_set_label(ui_data->query_button, CANCEL_TEXT);
			break;

		case QUERY_OFF:

			gtk_button_set_label(ui_data->query_button, QUERY_TEXT);
			break;

		case QUERY_FAILED:

			fprintf(stderr, "GNUNET_FS_get_indexed_files() error\n");
			gtk_button_set_label(ui_data->query_button, "Cerca XX");
			break;

		case QUERY_COMPLETED: {

			GtkTreeIter iter;

			gtk_button_set_label(ui_data->query_button, QUERY_TEXT);
			gtk_list_store_clear(ui_data->list_store);

			for (
				const GList * item = ui_data->app_data->fs_query.paths;
					item;
				item = item->next
			) {

				gtk_list_store_append(ui_data->list_store, &iter);

				gtk_list_store_set(
					ui_data->list_store,
					&iter,
					PF_COL_PATH, (gchar *) item->data,
					-1
				);

			}

			gtk_widget_set_visible(
				GTK_WIDGET(ui_data->reset_button),
				ui_data->app_data->fs_query.paths != NULL
			);

		}

	}

	g_mutex_unlock(&ui_data->app_data->fs_query.indexed_mutex);
	return false;

	#undef ui_data

}


/**

	@brief      Callback function for the `"List published files"` button
	@param      button          The `"List published files"` button
	@param      v_ui_data       The private UI data, passed as a `void *`
	                            pointer
	@return     Nothing

**/
static void on_list_files_clicked (
	GtkWidget * const button,
	gpointer v_app_data
) {

	#define app_data ((AppData *) v_app_data)

	g_mutex_lock(&app_data->fs_query.indexed_mutex);

	const bool b_must_cancel =
		app_data->fs_query.state == QUERY_RUNNING;

	g_mutex_unlock(&app_data->fs_query.indexed_mutex);

	GNUNET_WORKER_push_load_with_priority(
		app_data->gnunet_worker,
		GNUNET_SCHEDULER_PRIORITY_UI,
		b_must_cancel ?
			&cancel_indexed_query
		:
			&query_indexed_files,
		app_data
	);

	#undef app_data

}


/**

	@brief      Callback function for the `"Clear"` button
	@param      clear_btn       The `"Clear"` button
	@param      v_ui_data       The private UI data, passed as a `void *`
	                            pointer
	@return     Nothing

**/
static void on_clear_list_clicked (
	GtkWidget * const clear_btn,
	gpointer v_ui_data
) {

	#define ui_data ((UISession *) v_ui_data)

	gtk_list_store_clear(ui_data->list_store);
	gtk_widget_set_visible(clear_btn, false);

	#undef ui_data

}


/**

	@brief      The callback for the `GtkApplication`'s `"activate"` signal
	@param      app             The current `GtkApplication`
	@param      v_ui_data       The private UI data, passed as a `void *`
	                            pointer
	@return     Nothing

**/
static void on_foobar_app_activate (
	GtkApplication * app,
	gpointer v_ui_data
) {

	#define ui_data ((UISession *) v_ui_data)

	ui_data->list_store = gtk_list_store_new(PF_NUM_COLS, G_TYPE_STRING);
	ui_data->query_button = GTK_BUTTON(gtk_button_new_with_label(QUERY_TEXT));
	ui_data->reset_button = GTK_BUTTON(gtk_button_new_with_label(CLEAR_TEXT));

	GtkWidget
		* const window = gtk_application_window_new(app),
		* const header = gtk_header_bar_new(),
		* const scrolled = gtk_scrolled_window_new(),
		* const box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0),
		* const tree = gtk_tree_view_new_with_model(
			GTK_TREE_MODEL(ui_data->list_store)
		);

	GtkTreeViewColumn * const col = gtk_tree_view_column_new();
	GtkCellRenderer * const rend = gtk_cell_renderer_text_new();

	gtk_widget_set_visible(GTK_WIDGET(ui_data->reset_button), false);

	gtk_header_bar_set_title_widget(
		GTK_HEADER_BAR(header),
		gtk_label_new("Files published via GNUnet")
	);

	gtk_header_bar_pack_start(
		GTK_HEADER_BAR(header),
		GTK_WIDGET(ui_data->query_button)
	);

	gtk_header_bar_pack_start(
		GTK_HEADER_BAR(header),
		GTK_WIDGET(ui_data->reset_button)
	);

	gtk_window_set_titlebar(GTK_WINDOW(window), header);
	gtk_window_set_default_size(GTK_WINDOW(window), 800, 400);
	gtk_widget_set_halign(box, GTK_ALIGN_START);
	gtk_widget_set_valign(box, GTK_ALIGN_START);
	gtk_tree_view_column_set_title(col, "Path");
	gtk_tree_view_column_pack_start(col, rend, true);
	gtk_tree_view_column_set_attributes(col, rend, "text", PF_COL_PATH, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), col);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), tree);

	gtk_scrolled_window_set_min_content_width(
		GTK_SCROLLED_WINDOW(scrolled),
		800
	);

	gtk_scrolled_window_set_min_content_height(
		GTK_SCROLLED_WINDOW(scrolled),
		400
	);

	gtk_scrolled_window_set_propagate_natural_width(
		GTK_SCROLLED_WINDOW(scrolled),
		true
	);

	gtk_scrolled_window_set_propagate_natural_height(
		GTK_SCROLLED_WINDOW(scrolled),
		true
	);

	gtk_box_append(GTK_BOX(box), scrolled);
	gtk_window_set_child(GTK_WINDOW(window), box);

	g_signal_connect(
		ui_data->query_button,
		"clicked",
		G_CALLBACK(on_list_files_clicked),
		ui_data->app_data
	);

	g_signal_connect(
		ui_data->reset_button,
		"clicked",
		G_CALLBACK(on_clear_list_clicked),
		ui_data
	);

	gtk_widget_show(window);

	#undef ui_data

}


/**

	@brief      The GTK main function, equipped with a GNUnet worker thread
	@param      gnunet_worker   The current `GNUNET_WORKER_Handle`
	@param      v_app_data      The data shared between threads, passed as a
	                            `void *` pointer
	@return     Nothing

**/
void gtk_main_with_gnunet_worker (
    GNUNET_WORKER_Handle gnunet_worker,
    gpointer v_app_data
) {

	#define shared_data ((AppData *) v_app_data)

	UISession * ui_data = g_new(UISession, 1);
	ui_data->gnunet_worker = gnunet_worker;
	ui_data->app_data = (AppData *) v_app_data;
	shared_data->ui_private = ui_data;

	shared_data->ui_app = gtk_application_new(
		"org.gtk.foobar",
		G_APPLICATION_FLAGS_NONE
	);

	g_signal_connect(
		shared_data->ui_app,
		"activate",
		G_CALLBACK(on_foobar_app_activate),
		ui_data
	);

	atomic_store(&shared_data->ui_is_running, true);

	shared_data->gtk_status = g_application_run(
		G_APPLICATION(shared_data->ui_app),
		shared_data->argc,
		(char **) shared_data->argv
	);

	atomic_store(&shared_data->ui_is_running, false);
	g_free(ui_data);

	if (atomic_load(&shared_data->worker_is_running)) {

		GNUNET_WORKER_synch_destroy(gnunet_worker);

	}

	g_object_unref(shared_data->ui_app);
	printf("The GTK app has returned\n");

	#undef shared_data

}


/*  EOF  */

