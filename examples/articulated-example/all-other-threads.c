/*  -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */

/*  No function in this document is launched in the scheduler's thread  */

#include <stdbool.h>
#include <pthread.h>
#include <gnunet/gnunet_worker_lib.h>

#include "common.h"
#include "gnunet-thread.h"


/*  You can add as many threads as you want here...  */
static const char * const thread_names[] = {
	"thread one",
	"thread two",
	"thread three",
	"thread four",
	"thread five"
};


#define NUM_THREADS (sizeof(thread_names) / sizeof(char *))


/*  Shared data between threads  */
static ThreadData my_thread_data[sizeof(thread_names) / sizeof(char *)];


/*  All threads' main function  */
static void * thread_external_to_the_scheduler (
	void * const v_thread_data
) {

	#define thread_data ((ThreadData *) v_thread_data)

	printf(
		"This is a separate thread named \"%s\"\n\n",
		thread_data->name
	);

	/*  Launch `task_for_the_scheduler_1()` in the scheduler's thread  */
	GNUNET_WORKER_push_load(
		thread_data->worker,
		&task_for_the_scheduler_1,
		v_thread_data
	);

	return NULL;

	#undef thread_data

}


int main (const int argc, const char * const * const argv) {

	/*  Create a separate thread where GNUnet's scheduler is run  */
	GNUNET_WORKER_Handle * my_current_worker = GNUNET_WORKER_create(
		NULL,
		NULL,
		NULL
	);

	if (!my_current_worker) {

		fprintf(stderr, "Sorry, something went wrong :-(\n");
		return 1;

	}

	/*  Create one thread for each member of the `thread_names` array  */
	for (size_t idx = 0; idx < NUM_THREADS; idx++) {

		my_thread_data[idx].name = thread_names[idx];
		my_thread_data[idx].worker = my_current_worker;
		pthread_create(
			&my_thread_data[idx].thread,
			NULL,
			&thread_external_to_the_scheduler,
			&my_thread_data[idx]
		);

	}

	/*  Make sure that all the threads have had enough time to start...  */
	sleep(1);

	/*  Create one thread for each member of the `thread_names` array  */
	for (
		size_t idx = 0;
			idx < NUM_THREADS;
		pthread_join(my_thread_data[idx++].thread, NULL)
	);

	/*  Shutdown the scheduler  */
	GNUNET_WORKER_synch_destroy(my_current_worker);

}


/*  EOF  */

